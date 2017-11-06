#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include "master_send.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

static DWORD g_dwSeq = 0;

VOID *master_alloc_reqMsg(BYTE byMstAddr, BYTE bySlvAddr, WORD wCmd)
{
    WORD wMsgLen = 0;
    switch(wCmd)
    {
        case CMD_LOGIN:
            wMsgLen = sizeof(MSG_LOGIN_REQ_S);
            break;

        case CMD_KEEP_ALIVE:
            wMsgLen = sizeof(MSG_KEEP_ALIVE_REQ_S);
            break;

        default:
            return NULL;
    }

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)malloc(wMsgLen);
    if(pstMsgHdr)
    {
        pstMsgHdr->wSig  = htons(START_SIG_1);
        pstMsgHdr->wVer  = htons(VERSION_INT);
        pstMsgHdr->wSrcAddr = htons((DWORD)byMstAddr);
        pstMsgHdr->wDstAddr = htons((DWORD)bySlvAddr);
        pstMsgHdr->dwSeq = htonl(g_dwSeq++);
        pstMsgHdr->wCmd = htons(wCmd);
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

VOID *master_alloc_rspMsg(BYTE byMstAddr, BYTE bySlvAddr, WORD dwSeq, WORD wCmd)
{
    WORD wMsgLen = 0;
    switch(wCmd)
    {
        case CMD_LOGIN:
            wMsgLen = sizeof(MSG_LOGIN_RSP_S);
            break;

        case CMD_KEEP_ALIVE:
            wMsgLen = sizeof(MSG_KEEP_ALIVE_RSP_S);
            break;

        case CMD_DATA_BATCH:
            wMsgLen = sizeof(MSG_DATA_BATCH_RSP_S);
            break;

        case CMD_DATA_INSTANT:
            wMsgLen = sizeof(MSG_DATA_INSTANT_RSP_S);
            break;

        case CMD_DATA_WAITED:
            wMsgLen = sizeof(MSG_DATA_WAITED_RSP_S);
            break;

        default:
            return NULL;
    }

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)malloc(wMsgLen);
    if(pstMsgHdr)
    {
        pstMsgHdr->wSig  = htons(START_SIG_1);
        pstMsgHdr->wVer  = htons(VERSION_INT);
        pstMsgHdr->wSrcAddr = byMstAddr;
        pstMsgHdr->wDstAddr = bySlvAddr;
        pstMsgHdr->dwSeq = dwSeq;
        pstMsgHdr->wCmd = wCmd;
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

DWORD master_send(void *pArg, void *pData, WORD wDataLen, DWORD dwTimeout)
{
    
}


