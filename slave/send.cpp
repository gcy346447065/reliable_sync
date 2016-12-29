#include <netinet/in.h> //for sockaddr_in htons
#include <stdlib.h> //for malloc rand
#include <unistd.h> //for read write
#include <string.h> //for memset strstr memcpy
#include "log.h"
#include "timer.h"
#include "macro.h"
#include "send.h"

extern int g_iKeepaliveTimerFd;

static char g_cSeq = 0;

int send2MasterSync(int iSyncSockFd, const void *pMsg, int iMsgLen)
{
    if(write(iSyncSockFd, pMsg, iMsgLen) < 0) //after connect, write = send
    {
        log_debug("Send to SLAVE SYNC failed!");
        return -1;
    }

    timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE); //restart keepalive timer

    return 0;
}

MSG_HEADER *alloc_slave_reqMsg(char cCmd, int iLength)
{
    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iLength);

    if(pMsgHeader)
    {
        pMsgHeader->sSignature = htons(START_FLAG);
        pMsgHeader->cCmd = cCmd;
        pMsgHeader->cSeq = g_cSeq++;
        pMsgHeader->sLength = htons(iLength - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}

MSG_HEADER *alloc_slave_rspMsg(char cCmd, char cSeq)
{
    int iMsgLen = 0;
    switch(cCmd)
    {
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
        pMsgHeader->sLength = htons(iMsgLen - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}
