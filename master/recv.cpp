#include <netinet/in.h> //for sockaddr_in htons

#include "log.h"
#include "timer.h"
#include "protocol.h"
#include "sync.h"
#include "queue.h"
#include "recv.h"

extern char g_cMasterSyncStatus;
extern char g_cLoginRspSeq;
extern char g_cMasterSpecifyID;

extern int g_iLoginTimerFd;

extern stQueue *g_pstInstantQueue;
extern stQueue *g_pstWaitedQueue;

typedef int (*MSG_PROC)(const char *pcMsg);
typedef struct
{
    char cmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

static int sync_login(const char *pcMsg)
{
    const MSG_LOGIN_REQ *req = (const MSG_LOGIN_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(req->msgHeader.sLength) < sizeof(MSG_LOGIN_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    if(req->cSynFlag == 1 && req->cAckFlag == 0)
    {
        //get the first one in three-way handshake, loop send the second one as rsp
        log_info("sync_login loop send rsp.");

        g_cMasterSyncStatus = STATUS_LOGIN;
        g_cLoginRspSeq = req->msgHeader.cSeq;
        timer_start(g_iLoginTimerFd, 1);//get timer right now
    }
    else if(req->cSynFlag == 0 && req->cAckFlag == 1)
    {
        //get the third one in three-way handshake, login succeed
        log_info("sync_login succeed.");

        g_cMasterSyncStatus = STATUS_NEWCFG;
        timer_stop(g_iLoginTimerFd);
    }

    return 0;
}

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
        log_error("login message length not enough!");
        return -1;
    }

    if(rsp->cResult == NEWCFG_RESULT_SUCCEED)
    {
        log_info("get sync_newCfgInstant rsp succeed.");
    }
    else
    {
        log_info("get sync_newCfgInstant(newCfgID:%d) rsp failed(result:%d).", ntohl(rsp->iNewCfgID), rsp->cResult);

        //resend the new cfg with newCfgID
        queue_foreach(g_pstInstantQueue, ntohl(rsp->iNewCfgID));
    }

    return 0;
}

static int sync_newCfgWaited(const char *pcMsg)
{


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
        log_error("login message length not enough!");
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
        if(g_msgProcs[i].cmd == pcMsgHeader->cCmd)
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
