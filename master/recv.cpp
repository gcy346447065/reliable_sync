#include <netinet/in.h> //for sockaddr_in htons
#include <unistd.h> //for read write
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "event.h"
#include "protocol.h"
#include "instantList.h"
#include "waitedList.h"
#include "checksum.h"
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

extern stInstantList *g_pstInstantList;
extern stWaitedList *g_pstWaitedList;

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
    if(ntohl(req->msgHeader.iLength) < sizeof(MSG_LOGIN_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    int iRet = 0;

    if(req->cSynFlag == 1 && req->cAckFlag == 0 && g_cMasterSyncStatus == STATUS_INIT)
    {
        //get the first one in three-way handshake, loop send the second one as rsp
        log_info("sync_login loop send rsp.");

        //记录g_cSlaveSpecifyID，如果已有值且前后不一，说明对端重启
        if(g_cSlaveSpecifyID == 0)
        {
            g_cSlaveSpecifyID = req->cSpecifyID;
        }
        else if(g_cSlaveSpecifyID != req->cSpecifyID)
        {
            iRet = event_setEventFlags(g_iMainEventFd, MASTER_EVENT_SLAVE_RESTART);
            if(iRet < 0)
            {
                log_error("set g_iMainEventFd MASTER_EVENT_SLAVE_RESTART error(%d)!", iRet);
                return -1;
            }

            g_cSlaveSpecifyID = req->cSpecifyID;
        }

        g_cMasterSyncStatus = STATUS_LOGIN;
        g_cLoginRspSeq = req->msgHeader.cSeq;
        
        iRet = timer_start(g_iLoginSynAckTimerFd, LOGIN_TIMER_VALUE); //8s
        if(iRet < 0)
        {
            log_error("sync timer_start error(%d)!", iRet);
            return -1;
        }
    }
    else if(req->cSynFlag == 0 && req->cAckFlag == 1 && g_cMasterSyncStatus == STATUS_LOGIN)
    {
        //get the third one in three-way handshake, login succeed
        log_info("sync_login succeed.");

        iRet = timer_stop(g_iLoginSynAckTimerFd);
        if(iRet < 0)
        {
            log_error("sync timer_start error(%d)!", iRet);
            return -1;
        }

        g_cMasterSyncStatus = STATUS_NEWCFG;//如果停止定时器失败则不变换状态
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
    if(ntohl(rsp->msgHeader.iLength) < sizeof(MSG_NEWCFG_INSTANT_RSP) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    stInstantNode *pstNode = instantList_find(g_pstInstantList, ntohl(rsp->iInstantID));
    if(pstNode == NULL)
    {
        return -1;
    }

    if(rsp->cResult == NEWCFG_RESULT_SUCCEED)
    {
        log_info("get sync_newCfgInstant rsp succeed.");

        //成功，删除该节点
        instantList_delete(g_pstInstantList, pstNode);
    }
    else
    {
        log_info("get sync_newCfgInstant(newCfgID:%d) rsp failed(result:%d).", ntohl(rsp->iInstantID), rsp->cResult);

        if(pstNode->iSendTimers >= 3)
        {
            //重发3次仍失败，删除该节点
            instantList_delete(g_pstInstantList, pstNode);
        }
        else
        {
            //重发该节点
            MSG_NEWCFG_INSTANT_REQ *req = alloc_master_newCfgInstantReq(pstNode->pData, pstNode->iDataLen, pstNode->uiInstantID);
            if(req == NULL)
            {
                log_error("alloc_master_newCfgInstantReq error!");
                return -1;
            }

            if(sendToSlaveSync(g_iSyncSockFd, req, sizeof(MSG_NEWCFG_INSTANT_REQ) + pstNode->iDataLen) < 0)
            {
                log_debug("Send to SLAVE SYNC failed!");
            }

            pstNode->iFindTimers = 0;//查找次数清0
            pstNode->iSendTimers++;//发送次数累加
        }
    }

    return 0;
}

static int sync_newCfgWaited(const char *pcMsg)
{
    const MSG_NEWCFG_WAITED_REQ *rsp = (const MSG_NEWCFG_WAITED_REQ *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohl(rsp->msgHeader.iLength) < sizeof(MSG_NEWCFG_WAITED_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
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
    if(ntohl(req->msgHeader.iLength) < sizeof(MSG_KEEP_ALIVE_REQ) - MSG_HEADER_LEN)
    {
        log_error("keep alive message length not enough!");
        return -1;
    }

    //cSpecifyID前后不一，说明对端重启
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

    return -1;//未解析出函数说明异常
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
    while(iLeftLen >= ntohl(pcMsgHeader->iLength) + MSG_HEADER_LEN)
    {
        const unsigned char *status = (const unsigned char *)(&(pcMsgHeader->sSignature));
        if((status[0] != START_FLAG / 0x100) || (status[1] != START_FLAG % 0x100))
        {
            log_error("signature error(%x)!", (unsigned)ntohs(pcMsgHeader->sSignature));
            return -1;
        }

        short sChecksum = checksum((const char *)pcMsgHeader, MSG_HEADER_LEN + ntohl(pcMsgHeader->iLength))
        if(sChecksum != ntohs(pcMsgHeader->sChecksum))
        {
            log_error("checksum error(msg:%x, calc:%x)!", ntohs(pcMsgHeader->sChecksum), sChecksum);
            continue;
        }

        handle_one_msg((const char *)pcMsgHeader);//如果多个数据包中有一个数据包未找到相应的解析函数时，暂未记录此异常情况
        iLeftLen = iLeftLen - MSG_HEADER_LEN - ntohl(pcMsgHeader->iLength);
        pcMsgHeader = (const MSG_HEADER *)(pcMsg + iMsgLen - iLeftLen);
    }

    return 0;
}

