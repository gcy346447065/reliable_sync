#include <netinet/in.h> //for sockaddr_in htons
#include <unistd.h> //for read write
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "event.h"
#include "protocol.h"
#include "list.h"
#include "send.h"
#include "recv.h"

extern char g_cMasterSyncStatus;
extern char g_cLoginRspSeq;
extern char g_cMasterSpecifyID;
extern char g_cSlaveSpecifyID;

extern int g_iSyncSockFd;
extern int g_iLoginSynAckTimerFd;
extern int g_iCheckaliveTimerFd;
extern int g_iMainEventFd;

extern stList *g_pstInstantList;
extern stList *g_pstWaitedList;

typedef int (*MSG_PROC)(const char *pcMsg);
typedef struct
{
    char cCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;


int recvFromSlaveSync(int iSyncSockFd, void *pMsg, int iMaxMsgLen)
{
    int iRet = timer_start(g_iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return -1;
    }

    if((iRet = read(iSyncSockFd, pMsg, iMaxMsgLen)) < 0)
    {
        log_error("Recv from SLAVE SYNC error!");
    }
    return iRet;
}

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

    if(req->cSynFlag == 1 && req->cAckFlag == 0 && g_cMasterSyncStatus == STATUS_INIT)
    {
        //get the first one in three-way handshake, loop send the second one as rsp
        log_info("sync_login loop send rsp.");
        if(g_cSlaveSpecifyID == 0)
        {
            g_cSlaveSpecifyID = req->cSpecifyID;
        }
        else if(g_cSlaveSpecifyID != req->cSpecifyID)
        {
            int iRet = event_setEventFlags(g_iMainEventFd, MASTER_EVENT_SLAVE_RESTART);
            if(iRet < 0)
            {
                log_error("set g_iMainEventFd MASTER_EVENT_SLAVE_RESTART error(%d)!", iRet);
                return -1;
            }

            g_cSlaveSpecifyID = req->cSpecifyID;
        }

        g_cMasterSyncStatus = STATUS_LOGIN;
        g_cLoginRspSeq = req->msgHeader.cSeq;
        
        timer_start(g_iLoginSynAckTimerFd, LOGIN_TIMER_VALUE); //8s
    }
    else if(req->cSynFlag == 0 && req->cAckFlag == 1 && g_cMasterSyncStatus == STATUS_LOGIN)
    {
        //get the third one in three-way handshake, login succeed
        log_info("sync_login succeed.");

        timer_stop(g_iLoginSynAckTimerFd);
        g_cMasterSyncStatus = STATUS_NEWCFG;
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

        list_deleteByDataID(g_pstInstantList, ntohl(rsp->iNewCfgID));
    }
    else
    {
        log_info("get sync_newCfgInstant(newCfgID:%d) rsp failed(result:%d).", ntohl(rsp->iNewCfgID), rsp->cResult);

        //resend the new cfg with newCfgID
        stNode *pNode = list_find(g_pstInstantList, ntohl(rsp->iNewCfgID));
        if(pNode != NULL)
        {
            if(pNode->iSendTimers >= 3)//重发3次仍失败则删去节点
            {
                list_deleteByNode(g_pstInstantList, pNode);
            }
            else
            {
                MSG_NEWCFG_INSTANT_REQ *req = alloc_master_newCfgInstantReq(pNode->pData, pNode->iDataLen, pNode->iDataID);
                if(req == NULL)
                {
                    log_error("alloc_master_newCfgInstantReq error!");
                    return -1;
                }

                if(sendToSlaveSync(g_iSyncSockFd, req, sizeof(MSG_NEWCFG_INSTANT_REQ) + pNode->iDataLen) < 0)
                {
                    log_debug("Send to SLAVE SYNC failed!");
                }

                pNode->iFindTimers = 0;//查找次数清0
                pNode->iSendTimers++;//发送次数累加
            }
        }
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
        log_error("keep alive message length not enough!");
        return -1;
    }

    if(g_cSlaveSpecifyID != req->cSpecifyID)
    {
        int iRet = event_setEventFlags(g_iMainEventFd, MASTER_EVENT_SLAVE_RESTART);
        if(iRet < 0)
        {
            log_error("set g_iMainEventFd MASTER_EVENT_SLAVE_RESTART error(%d)!", iRet);
            return -1;
        }

        g_cSlaveSpecifyID = req->cSpecifyID;
    }

    MSG_KEEP_ALIVE_RSP *rsp = (MSG_KEEP_ALIVE_RSP *)alloc_master_rspMsg(CMD_KEEP_ALIVE, req->msgHeader.cSeq);
    if(rsp == NULL)
    {
        log_error("alloc_master_rspMsg MSG_KEEP_ALIVE_RSP error!");
        return -1;
    }
    rsp->cSpecifyID = g_cMasterSpecifyID;
    if(sendToSlaveSync(g_iSyncSockFd, rsp, sizeof(MSG_KEEP_ALIVE_RSP)) < 0)
    {
        log_debug("Send to SLAVE SYNC failed!");
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

    if(g_cMasterSyncStatus == STATUS_INIT || g_cMasterSyncStatus == STATUS_LOGIN)//只接受CMD_LOGIN消息
    {
        if(pcMsgHeader->cCmd == CMD_LOGIN)
        {
            return sync_login(pcMsg);
        }
    }
    else if(g_cMasterSyncStatus == STATUS_NEWCFG)//接受所有消息
    {
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

