#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include <stdio.h> // for sprintf
#include <fcntl.h> // for open
#include <unistd.h> // for write
#include "master_send.h"
#include "master.h"
#include "macro.h"
#include "memory.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

static DWORD g_dwSeq = 0;

static DWORD g_dwMstDataSeq = 1;
static DWORD g_dwMstInstantID = 0;

VOID *master_alloc_reqMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, WORD wCmd)
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

        case CMD_DATA_BATCH:
            wMsgLen = sizeof(MSG_DATA_BATCH_REQ_S);
            break;

        case CMD_DATA_INSTANT:
            wMsgLen = sizeof(MSG_DATA_INSTANT_REQ_S);
            break;

        case CMD_DATA_WAITED:
            wMsgLen = sizeof(MSG_DATA_WAITED_REQ_S);
            break;

        default:
            return NULL;
    }

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)malloc(wMsgLen);
    if(pstMsgHdr)
    {
        pstMsgHdr->wSig  = htons(wSig);
        pstMsgHdr->wVer  = htons(VERSION_INT);
        pstMsgHdr->wSrcAddr = htons(wSrcAddr);
        pstMsgHdr->wDstAddr = htons(wDstAddr);
        pstMsgHdr->dwSeq = htonl(g_dwSeq++);
        pstMsgHdr->wCmd = htons(wCmd);
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

VOID *master_alloc_rspMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, DWORD dwSeq, WORD wCmd)
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

        case CMD_GET_DATA_COUNT:
            wMsgLen = sizeof(MSG_GET_DATA_COUNT_RSP_S);
            break;

        default:
            return NULL;
    }

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)malloc(wMsgLen);
    if(pstMsgHdr)
    {
        pstMsgHdr->wSig  = htons(wSig);
        pstMsgHdr->wVer  = htons(VERSION_INT);
        pstMsgHdr->wSrcAddr = htons(wSrcAddr);
        pstMsgHdr->wDstAddr = htons(wDstAddr);
        pstMsgHdr->dwSeq = htonl(dwSeq);
        pstMsgHdr->wCmd = htons(wCmd);
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

void *master_alloc_dataBatch(WORD wSrcAddr, WORD wDstAddr, DWORD dwDataStart, DWORD dwDataEnd, 
                                    DWORD dwDataID, void *pBuf, WORD wBufLen)
{
    WORD wMsgLen = sizeof(MSG_DATA_BATCH_REQ_S) + wBufLen;
    MSG_DATA_BATCH_REQ_S *pstBatch = (MSG_DATA_BATCH_REQ_S *)malloc(wMsgLen);
    if(pstBatch)
    {
        pstBatch->stMsgHdr.wSig = htons(START_SIG_2);
        pstBatch->stMsgHdr.wVer = htons(VERSION_INT);
        pstBatch->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstBatch->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstBatch->stMsgHdr.dwSeq = htonl(g_dwMstDataSeq++);
        pstBatch->stMsgHdr.wCmd = htons(CMD_DATA_BATCH);
        pstBatch->stMsgHdr.wLen = htons(wMsgLen - MSG_HDR_LEN);

        pstBatch->stData.dwDataStart = htonl(dwDataStart);
        pstBatch->stData.dwDataEnd = htonl(dwDataEnd);
        pstBatch->stData.stData.dwDataID = htonl(dwDataID);
        pstBatch->stData.stData.wDataLen = htons(wBufLen);
        pstBatch->stData.stData.wDataChecksum = htons(0);
        memcpy(pstBatch->stData.stData.abyData, pBuf, wBufLen);
    }

    return (void *)pstBatch;
}

void *master_alloc_dataInstant(WORD wSrcAddr, WORD wDstAddr, DWORD dwDataID, void *pBuf, WORD wBufLen)
{
    WORD wMsgLen = sizeof(MSG_DATA_INSTANT_REQ_S) + wBufLen;
    MSG_DATA_INSTANT_REQ_S *pstInstant = (MSG_DATA_INSTANT_REQ_S *)malloc(wMsgLen);
    if(pstInstant)
    {
        pstInstant->stMsgHdr.wSig = htons(START_SIG_2);
        pstInstant->stMsgHdr.wVer = htons(VERSION_INT);
        pstInstant->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstInstant->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstInstant->stMsgHdr.dwSeq = htonl(g_dwMstDataSeq++);
        pstInstant->stMsgHdr.wCmd = htons(CMD_DATA_INSTANT);
        pstInstant->stMsgHdr.wLen = htons(wMsgLen - MSG_HDR_LEN);

        pstInstant->stData.dwDataID = htonl(dwDataID);
        pstInstant->stData.wDataLen = htons(wBufLen);
        pstInstant->stData.wDataChecksum = htons(0);
        memcpy(pstInstant->stData.abyData, pBuf, wBufLen);
    }

    return (void *)pstInstant;
}

void *master_alloc_dataWaited(WORD wSrcAddr, WORD wDstAddr, DWORD dwDataID, void *pBuf, WORD wBufLen)
{
    WORD wMsgLen = sizeof(MSG_DATA_WAITED_REQ_S) + wBufLen;
    MSG_DATA_WAITED_REQ_S *pstWaited = (MSG_DATA_WAITED_REQ_S *)malloc(wMsgLen);
    if(pstWaited)
    {
        pstWaited->stMsgHdr.wSig = htons(START_SIG_2);
        pstWaited->stMsgHdr.wVer = htons(VERSION_INT);
        pstWaited->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstWaited->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstWaited->stMsgHdr.dwSeq = htonl(g_dwMstDataSeq++);
        pstWaited->stMsgHdr.wCmd = htons(CMD_DATA_WAITED);
        pstWaited->stMsgHdr.wLen = htons(wMsgLen - MSG_HDR_LEN);

        pstWaited->stData.dwDataID = htonl(dwDataID);
        pstWaited->stData.wDataLen = htons(wBufLen);
        pstWaited->stData.wDataChecksum = htons(0);
        memcpy(pstWaited->stData.abyData, pBuf, wBufLen);
    }

    return (void *)pstWaited;
}

DWORD master_sendMsg(void *pMst, WORD wDstAddr, void *pData, WORD wDataLen)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    mbufer *pMbufer = pclsMst->pMbufer;
    BYTE byLogNum = pclsMst->byLogNum;
    //log_debug(byLogNum, "master_sendMsg().");

    /* 向指定地址发送数据 */
    dwRet = pMbufer->send_message(wDstAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }
        
    return SUCCESS;
}

DWORD master_sendToSlaves(void *pMst, void *pData, WORD wDataLen)
{
    /*DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    mbufer *pMbufer = pclsMst->pMbufer;
    BYTE byLogNum = pclsMst->byLogNum;
    vector<SLAVE_S> vecSlvs = pclsMst->vecSlvs;
    log_debug(byLogNum, "master_sendToSlaves().");

    WORD wSlvAddr = vecSlvs.front();
     向主机主备线程接收端口发送数据 
    dwRet = pMbufer->send_message(wSlvAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }*/
        

    return SUCCESS;
}


