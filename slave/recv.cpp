#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand NULL
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "send.h"
#include "recv.h"

extern char g_cSlaveSyncStatus;
extern char g_cMasterSpecifyID;
extern char g_cSlaveSpecifyID;

extern int g_iSyncSockFd;
extern int g_iLoginTimerFd;
extern int g_iMainEventFd;

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

        if(g_cMasterSpecifyID == 0)
        {
            g_cMasterSpecifyID = rsp->cSpecifyID;
        }
        else if(g_cMasterSpecifyID != rsp->cSpecifyID)
        {
            int iRet = event_setEventFlags(g_iMainEventFd, SLAVE_EVENT_MASTER_RESTART);
            if(iRet < 0)
            {
                log_error("set g_iMainEventFd SLAVE_EVENT_MASTER_RESTART error(%d)!", iRet);
                return -1;
            }

            g_cMasterSpecifyID = rsp->cSpecifyID;
        }

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
            log_debug("Send to MASTER SYNC failed!");
        }

        g_cSlaveSyncStatus = STATUS_NEWCFG;
    }

    return 0;
}

static int sync_newCfgInstant(const char *pcMsg)
{
    const MSG_NEWCFG_INSTANT_REQ *req = (const MSG_NEWCFG_INSTANT_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(req->msgHeader.sLength) < sizeof(MSG_NEWCFG_INSTANT_REQ) - MSG_HEADER_LEN)
    {
        log_error("message sLength not enough!");
        return -1;
    }

    log_info("req->iNewCfgID(%d), req->sChecksum(%d).", ntohl(req->iNewCfgID), ntohs(req->sChecksum));

    int iDataLen = ntohs(req->msgHeader.sLength) + MSG_HEADER_LEN - sizeof(MSG_NEWCFG_INSTANT_REQ);
    short sCalChecksum = checksum((const char *)req->acData, iDataLen);
    log_info("sCalChecksum(%d).", sCalChecksum);

    MSG_NEWCFG_INSTANT_RSP *rsp = (MSG_NEWCFG_INSTANT_RSP *)alloc_slave_rspMsg(CMD_NEWCFG_INSTANT, req->msgHeader.cSeq);
    if(!rsp)
    {
        log_error("msg rsp empty!");
        return -1;
    }
    rsp->iNewCfgID = req->iNewCfgID;//一次ntohl、一次htonl，所以省去转序

    if(sCalChecksum == (short)ntohs(req->sChecksum))
    {
        rsp->cResult = NEWCFG_RESULT_SUCCEED;

        //TODO:还原配置
        log_debug("TODO: here is new cfg.");
    }
    else
    {
        rsp->cResult = NEWCFG_RESULT_CHECKSUM_ERROR;

        log_debug("NEWCFG_RESULT_CHECKSUM_ERROR.");
    }

    if(send2MasterSync(g_iSyncSockFd, rsp, sizeof(MSG_NEWCFG_INSTANT_RSP)) < 0)
    {
        log_debug("Send to MASTER SYNC failed!");
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

    if(g_cMasterSpecifyID != req->cSpecifyID)
    {
        int iRet = event_setEventFlags(g_iMainEventFd, SLAVE_EVENT_MASTER_RESTART);
        if(iRet < 0)
        {
            log_error("set g_iMainEventFd SLAVE_EVENT_MASTER_RESTART error(%d)!", iRet);
            return -1;
        }

        g_cMasterSpecifyID = req->cSpecifyID;
    }

    MSG_KEEP_ALIVE_RSP *rsp = (MSG_KEEP_ALIVE_RSP *)alloc_slave_rspMsg(CMD_KEEP_ALIVE, sizeof(MSG_KEEP_ALIVE_RSP));
    if(rsp == NULL)
    {
        log_error("alloc_master_rspMsg MSG_KEEP_ALIVE_RSP error!");
        return -1;
    }
    rsp->cSpecifyID = g_cSlaveSpecifyID;
    if(send2MasterSync(g_iSyncSockFd, rsp, sizeof(MSG_KEEP_ALIVE_RSP)) < 0) 
    {
        log_debug("Send to MASTER SYNC failed!");
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
