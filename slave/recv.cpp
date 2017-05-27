#include <netinet/in.h> //for sockaddr_in htons
#include <unistd.h> //for read write
#include <string.h> //for memcpy
#include <stdio.h>
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "send.h"
#include "recv.h"
#include "instantList.h"
#include "waitedList.h"

extern char g_cSlaveSyncStatus;
extern char g_cMasterSpecifyID;
extern char g_cSlaveSpecifyID;

extern int g_iSyncSockFd;
extern int g_iLoginSynTimerFd;
extern int g_iCheckaliveTimerFd;
extern int g_iMainEventFd;

typedef int (*MSG_PROC)(const char *pcMsg);
typedef struct
{
    char cCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

int recvFromMasterSync(int iSyncSockFd, void *pMsg, int iMaxMsgLen)
{printf("recv from\n");
    int iRet = timer_start(g_iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return -1;
    }

    if((iRet = read(iSyncSockFd, pMsg, iMaxMsgLen)) < 0)
    {
        log_error("Recv from MASTER SYNC error!");
    }
    return iRet;
}

static int sync_login(const char *pcMsg)
{
    int iRet = 0;

    const MSG_LOGIN_RSP *rsp = (const MSG_LOGIN_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohl(rsp->msgHeader.iLength) < sizeof(MSG_LOGIN_RSP) - MSG_HEADER_LEN)
    {
        log_error("login msg length not enough!");
        return -1;
    }

    if(rsp->cSynAckFlag == 1 && g_cSlaveSyncStatus == STATUS_LOGIN)
    {
        iRet = timer_stop(g_iLoginSynTimerFd);
        if(iRet < 0)
        {
            log_error("login timer_stop error(%d)!", iRet);
            return -1;
        }

        if(g_cMasterSpecifyID == 0)
        {
            g_cMasterSpecifyID = rsp->cSpecifyID;
        }
        else if(g_cMasterSpecifyID != rsp->cSpecifyID)
        {
            iRet = event_setEventFlags(g_iMainEventFd, SLAVE_MAIN_EVENT_MASTER_RESTART);
            if(iRet < 0)
            {
                log_error("set g_iMainEventFd SLAVE_EVENT_MASTER_RESTART error(%d)!", iRet);
                return -1;
            }

            g_cMasterSpecifyID = rsp->cSpecifyID;
        }

        //get the second one in three-way handshake, send the third one as req
        MSG_LOGIN_REQ *req = (MSG_LOGIN_REQ *)alloc_slave_reqMsg(CMD_LOGIN);
        if(req == NULL)
        {
            log_error("alloc_slave_reqMsg MSG_LOGIN_REQ error!");
            return -1;
        }
        req->cSynFlag = 0;//the third one in three-way handshake
        req->cAckFlag = 1;
        req->cSpecifyID = g_cSlaveSpecifyID;
        if(sendToMasterSync(g_iSyncSockFd, req, sizeof(MSG_LOGIN_REQ)) < 0) //after connect, write = send
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
    if(ntohl(req->msgHeader.iLength) < sizeof(MSG_NEWCFG_INSTANT_REQ) - MSG_HEADER_LEN)
    {
        log_error("msg iLength not enough!");
        return -1;
    }

    log_info("req->uiInstantID(%d), req->sChecksum(%d).", ntohl(req->uiInstantID), ntohs(req->sChecksum));

    int iDataLen = ntohl(req->msgHeader.iLength) + MSG_HEADER_LEN - sizeof(MSG_NEWCFG_INSTANT_REQ);
    short sCalChecksum = checksum((const char *)req->acData, iDataLen);
    log_info("sCalChecksum(%d).", sCalChecksum);

    MSG_NEWCFG_INSTANT_RSP *rsp = (MSG_NEWCFG_INSTANT_RSP *)alloc_slave_rspMsg(CMD_NEWCFG_INSTANT, req->msgHeader.cSeq);
    if(!rsp)
    {
        log_error("msg rsp empty!");
        return -1;
    }
    rsp->uiInstantID = req->uiInstantID;//一次ntohl、一次htonl，所以省去转序

    int iRet = 0;
    if(sCalChecksum == (short)ntohs(req->sChecksum))
    {
        rsp->cResult = NEWCFG_RESULT_SUCCEED;

        log_info("Get instant new cfg.");

        //还原配置
        iRet = instantList_push((void *)req->acData, iDataLen);
        if(iRet != 0)
        {
            log_error("instantList_push error!");
            return -1;
        }

        iRet = event_setEventFlags(g_iMainEventFd, SLAVE_MAIN_EVENT_NEWCFG_INSTANT);
        if(iRet != 0)
        {
            log_error("set event flag SLAVE_MAIN_EVENT_NEWCFG_INSTANT failed!");
            return -1;
        }
    }
    else
    {
        rsp->cResult = NEWCFG_RESULT_CHECKSUM_ERROR;

        log_debug("NEWCFG_RESULT_CHECKSUM_ERROR.");
    }

    if(sendToMasterSync(g_iSyncSockFd, rsp, sizeof(MSG_NEWCFG_INSTANT_RSP)) < 0)
    {
        log_debug("Send to MASTER SYNC failed!");
    }

    return 0;
}

static int sync_newCfgWaited(const char *pcMsg)
{printf("get into sync_newcfgwaited\n");
    const MSG_NEWCFG_WAITED_REQ *req = (const MSG_NEWCFG_WAITED_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohl(req->msgHeader.iLength) < sizeof(MSG_NEWCFG_WAITED_REQ) - MSG_HEADER_LEN)
    {
        log_error("msg iLength not enough!");
        return -1;
    }

    log_info("ALL checksum in msg(%d).", ntohs(req->sChecksum));
    int iDataLen = ntohl(req->msgHeader.iLength) + MSG_HEADER_LEN - sizeof(MSG_NEWCFG_WAITED_REQ);
    short sAllCalChecksum = checksum((const char *)req->dataNewcfg, iDataLen);
    log_info("ALL checksum for calculate(%d).", sAllCalChecksum);

    if(sAllCalChecksum != (short)ntohs(req->sChecksum))
    {
        //配置包整体校验错误，不回复
        return 0;
    }

    //配置包整体校验正确，逐个校验单个配置
    const DATA_NEWCFG *pDataNewcfg = req->dataNewcfg;
    unsigned int uiSucceedSum = 0, uiWaitedSum = ntohl(req->uiWaitedSum);
    unsigned int auiSucceedID[uiWaitedSum];//最大也只可能是所有的配置都成功

    int iRet = 0;
    while(uiWaitedSum--)//循环直到等于配置包总长度
    {
        log_info("ONCE checksum in msg(%d).", ntohs(pDataNewcfg->sChecksum));
        short sOnceCalChecksum = checksum((const char *)pDataNewcfg->acData, ntohl(pDataNewcfg->iDataLen));
        log_info("ONCE checksum for calculate(%d).", sOnceCalChecksum);

        if(sOnceCalChecksum == (short)ntohs(pDataNewcfg->sChecksum))
        {
            //单个配置校验正确，记录下此ID
            auiSucceedID[uiSucceedSum++] = pDataNewcfg->uiWaitedID;//这里不转字节序，因为发送时还要再转成网络序

            log_info("Get waited new cfg.");printf("before waited push\n");

            //还原配置
            iRet = waitedList_push((void *)pDataNewcfg->acData, ntohl(pDataNewcfg->iDataLen), ntohl(pDataNewcfg->uiWaitedID));printf("after waited push\n");
            if(iRet != 0)
            {
                log_error("instantList_push error!");
            }
        }

        pDataNewcfg = (const DATA_NEWCFG *)(((const char *)pDataNewcfg) + sizeof(DATA_NEWCFG) + ntohl(pDataNewcfg->iDataLen));
    }

    MSG_NEWCFG_WAITED_RSP *rsp = alloc_slave_newCfgWaitedRsp(req->msgHeader.cSeq, sizeof(MSG_NEWCFG_WAITED_RSP) + uiSucceedSum * sizeof(unsigned int));
    if(!rsp)
    {
        log_error("msg rsp empty!");
        return -1;
    }
    memcpy(rsp->auiSucceedID, auiSucceedID, uiSucceedSum * sizeof(unsigned int));

    if(sendToMasterSync(g_iSyncSockFd, rsp, sizeof(MSG_NEWCFG_WAITED_RSP) + uiSucceedSum * sizeof(unsigned int)) < 0)
    {
        log_debug("Send to MASTER SYNC failed!");
    }

    //发送回应包后再触发事件，避免触发事件失败导致不回应
    iRet = event_setEventFlags(g_iMainEventFd, SLAVE_MAIN_EVENT_NEWCFG_WAITED);
    if(iRet != 0)
    {
        log_error("set event flag SLAVE_MAIN_EVENT_NEWCFG_WAITED failed!");
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
        log_error("msg length not enough!");
        return -1;
    }

    if(g_cMasterSpecifyID != req->cSpecifyID)
    {
        int iRet = event_setEventFlags(g_iMainEventFd, SLAVE_MAIN_EVENT_MASTER_RESTART);
        if(iRet < 0)
        {
            log_error("set g_iMainEventFd SLAVE_MAIN_EVENT_MASTER_RESTART error(%d)!", iRet);
            return -1;
        }

        g_cMasterSpecifyID = req->cSpecifyID;
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
{printf("get into handle one msg\n");
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    if(g_cSlaveSyncStatus == STATUS_LOGIN)//只接受CMD_LOGIN消息
    {
        if(pcMsgHeader->cCmd == CMD_LOGIN)
        {
            return sync_login(pcMsg);
        }
    }
    else if(g_cSlaveSyncStatus == STATUS_NEWCFG)//接受所有消息
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
{printf("get into handle sync msg\n");
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;printf("ok\n");

    if(iMsgLen < MSG_HEADER_LEN)
    {printf("ok1\n");
        log_error("sync message length not enough(%u<%u)", iMsgLen, MSG_HEADER_LEN);
        return -1;
    }

    int iLeftLen = iMsgLen;printf("ok2\n");printf("ileftlen:%d, ilength:%d, hearlen:%d\n", iLeftLen, ntohl(pcMsgHeader->iLength), MSG_HEADER_LEN);
    while(iLeftLen >= ntohl(pcMsgHeader->iLength) + MSG_HEADER_LEN)
    {printf("ok3\n");
        const unsigned char *status = (const unsigned char *)(&(pcMsgHeader->sSignature));
        if((status[0] != START_FLAG / 0x100) || (status[1] != START_FLAG % 0x100))
        {printf("ok4\n");
            log_error("receive message header sSignature error:%x", (unsigned)ntohs(pcMsgHeader->sSignature));
            return -1;
        }
        handle_one_msg((const char *)pcMsgHeader);printf("ok5\n");
        iLeftLen = iLeftLen - MSG_HEADER_LEN - ntohl(pcMsgHeader->iLength);
        pcMsgHeader = (const MSG_HEADER *)(pcMsg + iMsgLen - iLeftLen);
    }printf("ok6\n");

    return 0;
}
