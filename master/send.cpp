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

int send2SlaveSync(int iSyncSockFd, const void *pMsg, int iMsgLen)
{
    if(write(iSyncSockFd, pMsg, iMsgLen) < 0) //after connect, write = send
    {
        log_debug("Send to SLAVE SYNC failed!");
        return -1;
    }

    timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE); //restart keepalive timer

    return 0;
}

int alloc_master_rspMsg(char cCmd, char cSeq, void **ppMsg)
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
            return -1;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    pMsgHeader->sSignature = htons(START_FLAG);
    pMsgHeader->cCmd = cCmd;
    pMsgHeader->cSeq = cSeq;
    pMsgHeader->sLength = htons(iMsgLen - MSG_HEADER_LEN);

    log_hex(pMsgHeader, iMsgLen);
    *ppMsg = pMsgHeader;
    return 0;
}

int alloc_master_reqMsg(char cCmd, void **ppMsg)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
        case CMD_KEEP_ALIVE:
            iMsgLen = sizeof(MSG_KEEP_ALIVE_REQ);
            break;

        default:
            return -1;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    if(!pMsgHeader)
    {
        return -1;
    }

    pMsgHeader->sSignature = htons(START_FLAG);
    pMsgHeader->cCmd = cCmd;
    pMsgHeader->cSeq = ++g_cSeq;
    pMsgHeader->sLength = htons(iMsgLen - MSG_HEADER_LEN);

    log_hex(pMsgHeader, iMsgLen);
    *ppMsg = pMsgHeader;
    return 0;
}

int alloc_master_newCfgInstantReq(void *pData, int iDataLen, int iNewCfgID, void **ppMsg, int *piMsgLen)
{
    int iMsgLen = sizeof(MSG_NEWCFG_INSTANT_REQ) + iDataLen;

    MSG_NEWCFG_INSTANT_REQ *req = (MSG_NEWCFG_INSTANT_REQ *)malloc(iMsgLen);
    if(!req)
    {
        return -1;
    }

    req->msgHeader.sSignature = htons(START_FLAG);
    req->msgHeader.cCmd = CMD_NEWCFG_INSTANT;
    req->msgHeader.cSeq = ++g_cSeq;
    req->msgHeader.sLength = htons(iMsgLen - MSG_HEADER_LEN);

    req->iNewCfgID = htonl(iNewCfgID);
    req->sChecksum = htons(checksum((const char *)pData, iDataLen));
    memcpy(req->acData, pData, iDataLen);

    log_hex(req, iMsgLen);
    *ppMsg = req;
    *piMsgLen = iMsgLen;
    return 0;
}

int alloc_master_newCfgWaitedReq(int iMsgLen ,void **ppMsg, int *piMsgLen)
{
    MSG_NEWCFG_WAITED_REQ *req = (MSG_NEWCFG_WAITED_REQ *)malloc(iMsgLen);
    if(!req)
    {
        return -1;
    }

    req->msgHeader.sSignature = htons(START_FLAG);
    req->msgHeader.cCmd = CMD_NEWCFG_WAITED;
    req->msgHeader.cSeq = ++g_cSeq;
    req->msgHeader.sLength = htons(iMsgLen - MSG_HEADER_LEN);

    log_hex(req, iMsgLen);
    *ppMsg = req;
    *piMsgLen = iMsgLen;
    return 0;
}
