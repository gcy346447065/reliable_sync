#include <netinet/in.h> //for sockaddr_in htons

#include "log.h"
#include "timer.h"
#include "protocol.h"
#include "sync.h"
#include "recv.h"

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
    if(ntohs(req->header.length) < sizeof(MSG_LOGIN_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    if(req->synFlag == 1 && req->ackFlag == 0)
    {
        //get the first one in three-way handshake, loop send the second one as rsp
        log_info("sync_login loop send rsp.");

        g_masterSyncStatus = STATUS_LOGIN;
        g_loginRspSeq = req->header.seq;
        timer_start(g_iLoginTimerFd, 1);//get timer right now
    }
    else if(req->synFlag == 0 && req->ackFlag == 1)
    {
        //get the third one in three-way handshake, login succeed
        log_info("sync_login succeed.");

        g_masterSyncStatus = STATUS_NEW_CFG;
        timer_stop(g_iLoginTimerFd);
    }

    return 0;
}

static int sync_newCfg(const char *pcMsg)
{
    const MSG_NEW_CFG_RSP *rsp = (const MSG_NEW_CFG_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(rsp->header.length) < sizeof(MSG_NEW_CFG_RSP) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    if(rsp->result == NEW_CFG_RESULT_SUCCEED)
    {
        log_info("get sync_newCfg rsp succeed.");
    }
    else
    {
        log_info("get sync_newCfg(newCfgID:%d) rsp failed(result:%d).", rsp->newCfgID, rsp->result);

        //resend the new cfg with newCfgID
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
    if(ntohs(req->header.length) < sizeof(MSG_KEEP_ALIVE_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    return 0;
}

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             sync_login},
    {CMD_NEW_CFG,           sync_newCfg},
    {CMD_KEEP_ALIVE,        sync_keepAlive}
};

int handle_one_msg(const char *pcMsg)
{
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    for(int i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].cmd == pcMsgHeader->cmd)
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
    while(iLeftLen >= ntohs(pcMsgHeader->length) + MSG_HEADER_LEN)
    {
        const unsigned char *status = (const unsigned char *)(&(pcMsgHeader->signature));
        if((status[0] != START_FLAG / 0x100) || (status[1] != START_FLAG % 0x100))
        {
            log_error("receive message header signature error:%x", (unsigned)ntohs(pcMsgHeader->signature));
            return -1;
        }
        handle_one_msg((const char *)pcMsgHeader);
        iLeftLen = iLeftLen - MSG_HEADER_LEN - ntohs(pcMsgHeader->length);
        pcMsgHeader = (const MSG_HEADER *)(pcMsg + iMsgLen - iLeftLen);
    }

    return 0;
}
