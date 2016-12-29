#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand NULL
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "protocol.h"
#include "send.h"
#include "recv.h"

extern char g_cSlaveSyncStatus;
extern char g_cMasterSpecifyID;
extern char g_cSlaveSpecifyID;

extern int g_iSyncSockFd;
extern int g_iLoginTimerFd;

typedef int (*MSG_PROC)(const char *pcMsg);
typedef struct
{
    char cCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

static int sync_login(const char *pcMsg)
{
    const MSG_LOGIN_RSP *rsp = (const MSG_LOGIN_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(rsp->msgHeader.sLength) < sizeof(MSG_LOGIN_RSP) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    if(rsp->cSynAckFlag == 1 && g_cSlaveSyncStatus == STATUS_LOGIN)
    {
        timer_stop(g_iLoginTimerFd);

        //get the second one in three-way handshake, send the third one as req
        MSG_LOGIN_REQ *req = (MSG_LOGIN_REQ *)alloc_slave_reqMsg(CMD_LOGIN, sizeof(MSG_LOGIN_REQ));
        if(req == NULL)
        {
            log_error("alloc_slave_reqMsg MSG_LOGIN_REQ error!");
            return -1;
        }
        req->cSynFlag = 0;//the third one in three-way handshake
        req->cAckFlag = 1;
        req->cSpecifyID = g_cSlaveSpecifyID;
        if(send2MasterSync(g_iSyncSockFd, req, sizeof(MSG_LOGIN_REQ)) < 0) //after connect, write = send
        {
            log_debug("Send to SLAVE SYNC failed!");
        }

        g_cSlaveSyncStatus = STATUS_NEWCFG;
    }

    return 0;
}

#if 0
static int sync_newCfg(const char *pcMsg)
{
    const MSG_NEW_CFG_REQ *req = (const MSG_NEW_CFG_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg req empty!");
        return -1;
    }
    if(ntohs(req->msgHeader.sLength) < sizeof(MSG_NEW_CFG_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    log_info("req->newCfgNum(%d), req->checksum(%d).", ntohs(req->newCfgNum), ntohs(req->checksum));

    MSG_NEW_CFG_RSP *rsp = (MSG_NEW_CFG_RSP *)alloc_slave_rspMsg((const MSG_HEADER *)pcMsg);
    if(!rsp)
    {
        log_error("msg rsp empty!");
        return -1;
    }
    rsp->newCfgNum = req->newCfgNum;

    if(g_slaveSyncStatus == STATUS_NEW_CFG)
    {
        short iSlaveChecksum = checksum((const char *)(req->data), ntohs(req->msgHeader.sLength) - sizeof(short)*2);
        log_info("iSlaveChecksum(%d)", iSlaveChecksum);
        if(iSlaveChecksum == ntohs(req->checksum))
        {
            rsp->result = NEW_CFG_RESULT_SUCCEED;
        }
        else
        {
            rsp->result = NEW_CFG_RESULT_CHECKSUM_ERROR;
        }
    }
    else
    {
        rsp->result = NEW_CFG_RESULT_STATUS_ERROR;
    }

    if(write(iSockFd, rsp, sizeof(MSG_NEW_CFG_RSP)) < 0) //after connect, write = send
    {
        log_debug("Send to MASTER MSG_NEW_CFG_RSP failed!");
        return -1;
    }

    return 0;
}
#endif

static int sync_newCfgInstant(const char *pcMsg)
{
    const MSG_NEWCFG_INSTANT_RSP *rsp = (const MSG_NEWCFG_INSTANT_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(rsp->msgHeader.sLength) < sizeof(MSG_NEWCFG_INSTANT_RSP) - MSG_HEADER_LEN)
    {
        log_error("keep alive message sLength not enough!");
        return -1;
    }

    return 0;
}

static int sync_newCfgWaited(const char *pcMsg)
{
    const MSG_NEWCFG_WAITED_RSP *rsp = (const MSG_NEWCFG_WAITED_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(rsp->msgHeader.sLength) < sizeof(MSG_NEWCFG_WAITED_RSP) - MSG_HEADER_LEN)
    {
        log_error("keep alive message sLength not enough!");
        return -1;
    }

    return 0;
}

static int sync_keepAlive(const char *pcMsg)
{
    const MSG_KEEP_ALIVE_REQ *req = (const MSG_KEEP_ALIVE_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(req->msgHeader.sLength) < sizeof(MSG_KEEP_ALIVE_REQ) - MSG_HEADER_LEN)
    {
        log_error("keep alive message length not enough!");
        return -1;
    }

    return 0;
}

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             sync_login},
    {CMD_NEWCFG_INSTANT,    sync_newCfgInstant},
    {CMD_NEWCFG_WAITED,     sync_newCfgWaited},
    {CMD_KEEP_ALIVE,        sync_keepAlive}
};

int handle_one_msg(const char *pcMsg)
{
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    for(int i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].cCmd == pcMsgHeader->cCmd)
        {
            MSG_PROC pfn = g_msgProcs[i].pfn;
            if(pfn)
            {
                return pfn(pcMsg);
            }
        }
    }

    return -1;
}

int handle_sync_msg(const char *pcMsg, int iMsgLen)
{
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    if(iMsgLen < MSG_HEADER_LEN)
    {
        log_error("sync message length not enough(%u<%u)", iMsgLen, MSG_HEADER_LEN);
        return -1;
    }

    int iLeftLen = iMsgLen;
    while(iLeftLen >= ntohs(pcMsgHeader->sLength) + MSG_HEADER_LEN)
    {
        const unsigned char *status = (const unsigned char *)(&(pcMsgHeader->sSignature));
        if((status[0] != START_FLAG / 0x100) || (status[1] != START_FLAG % 0x100))
        {
            log_error("receive message header sSignature error:%x", (unsigned)ntohs(pcMsgHeader->sSignature));
            return -1;
        }
        handle_one_msg((const char *)pcMsgHeader);
        iLeftLen = iLeftLen - MSG_HEADER_LEN - ntohs(pcMsgHeader->sLength);
        pcMsgHeader = (const MSG_HEADER *)(pcMsg + iMsgLen - iLeftLen);
    }

    return 0;
}
