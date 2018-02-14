#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include <cstdlib> //for rand
#include <ctime> //for rand
#include "slave.h"
#include "slave_send.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

static WORD g_dwSlvDataSeq = 1;

VOID *slave_alloc_reqMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, WORD wCmd)
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
        pstMsgHdr->dwSeq = htonl(g_dwSlvDataSeq++);
        pstMsgHdr->wCmd = htons(wCmd);
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

VOID *slave_alloc_rspMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, DWORD dwSeq, WORD wCmd)
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
            wMsgLen = sizeof(MSG_DATA_SLAVE_BATCH_RSP_S);   //batch的回复包需要单独考虑，在外面赋值
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

VOID *slave_alloc_dataBatchRsp(WORD wSrcAddr, WORD wDstAddr, DWORD dwSeq, DWORD dwDataNum, DWORD dwDataStart, DWORD dwDataEnd)
{
    WORD wMsgLen = sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) + (dwDataNum * sizeof(DWORD));

    MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (MSG_DATA_SLAVE_BATCH_RSP_S *)malloc(wMsgLen);
    if(pstRsp)
    {
        pstRsp->stMsgHdr.wSig  = htons(START_SIG_2);
        pstRsp->stMsgHdr.wVer  = htons(VERSION_INT);
        pstRsp->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstRsp->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstRsp->stMsgHdr.dwSeq = htonl(dwSeq);
        pstRsp->stMsgHdr.wCmd = htons(CMD_DATA_BATCH);
        pstRsp->stMsgHdr.wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    pstRsp->stSlvRecvResult.dwDataStart = htonl(dwDataStart);
    pstRsp->stSlvRecvResult.dwDataEnd = htonl(dwDataEnd);
    pstRsp->stSlvRecvResult.dwNeedPkgNums = htonl(dwDataNum);

    return (VOID *)pstRsp;
}

DWORD slave_sendMsg(void *pSlv, WORD wDstAddr, void *pData, WORD wDataLen)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    mbufer *pMbufer = pclsSlv->pMbufer;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave_sendMsg().");
    
    /*取得[0,b)的随机整数：
    WORD wRandNum = rand()%(100 - 0) + 0;
    if(wRandNum < MSG_LOSE_RATE)
    {
        MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)pData;
        pstMsgHdr->wSig = START_SIG_4;
    }*/

    /* 向指定地址发送数据 */
    dwRet = pMbufer->send_message(wDstAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }
        
    return SUCCESS;
}

