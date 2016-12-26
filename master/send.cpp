#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand
#include <unistd.h> //for read write
#include <string.h> //for memset strstr memcpy
#include "log.h"
#include "sync.h"
#include "timer.h"
#include "checksum.h"
#include "macro.h"
#include "send.h"


static char g_seq = 0;

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

int alloc_master_rspMsg(char cmd, char seq, void **ppMsg)
{
    int iMsgLen = 0;
    switch(cmd)
    {
        case CMD_LOGIN:
            iMsgLen = sizeof(MSG_LOGIN_RSP);
            break;

        case CMD_NEW_CFG:
            iMsgLen = sizeof(MSG_NEW_CFG_RSP);
            break;

        case CMD_KEEP_ALIVE:
            iMsgLen = sizeof(MSG_KEEP_ALIVE_RSP);
            break;

        default:
            return -1;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    pMsgHeader->signature = htons(START_FLAG);
    pMsgHeader->cmd = cmd;
    pMsgHeader->seq = seq;
    pMsgHeader->length = htons(iMsgLen - MSG_HEADER_LEN);

    log_hex(pMsgHeader, iMsgLen);
    *ppMsg = pMsgHeader;
    return 0;
}

int alloc_master_reqMsg(char cmd, void **ppMsg)
{
    int iMsgLen = 0;
    switch(cmd)
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

    pMsgHeader->signature = htons(START_FLAG);
    pMsgHeader->cmd = cmd;
    pMsgHeader->seq = ++g_seq;
    pMsgHeader->length = htons(iMsgLen - MSG_HEADER_LEN);

    log_hex(pMsgHeader, iMsgLen);
    *ppMsg = pMsgHeader;
    return 0;
}

int alloc_master_newCfgReq(void *pData, int iDataLen, void **ppMsg, int *piMsgLen)
{
    int iMsgLen = sizeof(MSG_NEW_CFG_REQ) + iDataLen;

    MSG_NEW_CFG_REQ *req = (MSG_NEW_CFG_REQ *)malloc(iMsgLen);
    if(!req)
    {
        return -1;
    }

    req->header.signature = htons(START_FLAG);
    req->header.cmd = CMD_NEW_CFG;
    req->header.seq = ++g_seq;
    req->header.length = htons(iMsgLen - MSG_HEADER_LEN);

    req->newCfgID = htons(++g_newCfgID);
    req->checksum = htons(checksum((const char *)pData, iDataLen));
    memcpy(req->data, pData, iDataLen);

    log_hex(req, iMsgLen);
    *ppMsg = req;
    *piMsgLen = iMsgLen;
    return 0;
}
