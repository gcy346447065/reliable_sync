#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand NULL
#include <unistd.h> //for read write
#include <string.h> //for memset strstr memcpy
#include "log.h"
#include "timer.h"
#include "checksum.h"
#include "macro.h"
#include "send.h"

extern int g_iKeepaliveTimerFd;

static char g_cSeq = 0;

int sendToSlaveSync(int iSyncSockFd, void *pMsg, int iMsgLen)
{
    int iRet = timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE); //restart keepalive timer
    if(iRet < 0)
    {
        log_error("timer_start error!");
        return -1;
    }

    if((iRet = write(iSyncSockFd, pMsg, iMsgLen)) < 0) //after connect, write = send
    {
        log_error("Send to SLAVE SYNC error!");
    }
    return iRet;
}

MSG_HEADER *alloc_master_rspMsg(char cCmd, char cSeq)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
        case CMD_LOGIN:
            iMsgLen = sizeof(MSG_LOGIN_RSP);
            break;

        case CMD_NEWCFG_INSTANT:
            iMsgLen = sizeof(MSG_NEWCFG_INSTANT_RSP);
            break;

        case CMD_KEEP_ALIVE:
            iMsgLen = sizeof(MSG_KEEP_ALIVE_RSP);
            break;

        default:
            return NULL;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    if(pMsgHeader)
    {
        pMsgHeader->sSignature = htons(START_FLAG);
        pMsgHeader->cCmd = cCmd;
        pMsgHeader->cSeq = cSeq;
        pMsgHeader->iLength = htonl(iMsgLen - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}

MSG_HEADER *alloc_master_reqMsg(char cCmd)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
        case CMD_KEEP_ALIVE:
            iMsgLen = sizeof(MSG_KEEP_ALIVE_REQ);
            break;

        default:
            return NULL;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    if(pMsgHeader)
    {
        pMsgHeader->sSignature = htons(START_FLAG);
        pMsgHeader->cCmd = cCmd;
        pMsgHeader->cSeq = ++g_cSeq;
        pMsgHeader->iLength = htonl(iMsgLen - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}

MSG_NEWCFG_INSTANT_REQ *alloc_master_newCfgInstantReq(void *pData, int iDataLen, unsigned int uiInstantID)
{
    int iMsgLen = sizeof(MSG_NEWCFG_INSTANT_REQ) + iDataLen;

    MSG_NEWCFG_INSTANT_REQ *req = (MSG_NEWCFG_INSTANT_REQ *)malloc(iMsgLen);
    if(req)
    {
        req->msgHeader.sSignature = htons(START_FLAG);
        req->msgHeader.cCmd = CMD_NEWCFG_INSTANT;
        req->msgHeader.cSeq = ++g_cSeq;
        req->msgHeader.iLength = htonl(iMsgLen - MSG_HEADER_LEN);

        req->uiInstantID = htonl(uiInstantID);
        memcpy(req->acData, pData, iDataLen);
    }

    return req;
}

MSG_NEWCFG_WAITED_REQ *alloc_master_newCfgWaitedReq(int iMsgLen)
{
    MSG_NEWCFG_WAITED_REQ *req = (MSG_NEWCFG_WAITED_REQ *)malloc(iMsgLen);
    if(req)
    {
        req->msgHeader.sSignature = htons(START_FLAG);
        req->msgHeader.cCmd = CMD_NEWCFG_WAITED;
        req->msgHeader.cSeq = ++g_cSeq;
        req->msgHeader.iLength = htonl(iMsgLen - MSG_HEADER_LEN);
    }

    return 0;
}
