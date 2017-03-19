#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand
#include <unistd.h> //for read write
#include <string.h> //for memset strstr memcpy
#include <stdio.h>
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "send.h"

extern int g_iKeepaliveTimerFd;

static char g_cSeq = 0;

int sendToMasterSync(int iSyncSockFd, const void *pMsg, int iMsgLen)
{
    int iRet = timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE); //restart keepalive timer
    if(iRet < 0)
    {
        log_error("timer_start error!");
        return -1;
    }

    if((iRet = write(iSyncSockFd, pMsg, iMsgLen)) < 0) //after connect, write = send
    {
        log_error("Send to MASTER SYNC error!");
    }

    return iRet;
}

MSG_HEADER *alloc_slave_reqMsg(char cCmd)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
        case CMD_LOGIN:
            iMsgLen = sizeof(MSG_LOGIN_REQ);
            break;

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
        pMsgHeader->cSeq = g_cSeq++;
        pMsgHeader->iLength = htonl(iMsgLen - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}

MSG_HEADER *alloc_slave_rspMsg(char cCmd, char cSeq)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
        case CMD_NEWCFG_INSTANT:
            iMsgLen = sizeof(MSG_NEWCFG_INSTANT_RSP);
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

MSG_NEWCFG_WAITED_RSP *alloc_slave_newCfgWaitedRsp(char cSeq, unsigned int uiMsgLen)
{
    MSG_NEWCFG_WAITED_RSP *rsp = (MSG_NEWCFG_WAITED_RSP *)malloc(uiMsgLen);
    if(rsp)
    {
        rsp->msgHeader.sSignature = htons(START_FLAG);
        rsp->msgHeader.cCmd = CMD_NEWCFG_WAITED;
        rsp->msgHeader.cSeq = cSeq;
        rsp->msgHeader.iLength = htonl(uiMsgLen - MSG_HEADER_LEN);
    }

    return rsp;
}
