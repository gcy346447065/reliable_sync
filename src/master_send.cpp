#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include "master_send.h"
#include "master.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

static DWORD g_dwSeq = 0;

VOID *master_alloc_reqMsg(WORD wSrcAddr, WORD wDstAddr, WORD wCmd)
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

DWORD master_sendToTask(void *pMst, void *pData, WORD wDataLen)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    mbufer *pMbufer = pclsMst->pMbufer;
    BYTE byLogNum = pclsMst->byLogNum;
    WORD wTaskAddr = pclsMst->wTaskAddr;
    log_debug(byLogNum, "master_sendToTask().");

    /* 向主机主备线程接收端口发送数据 */
    dwRet = pMbufer->send_message(wTaskAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }
        
    return SUCCESS;
}

DWORD master_sendToSlaves(void *pMst, void *pData, WORD wDataLen)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    mbufer *pMbufer = pclsMst->pMbufer;
    BYTE byLogNum = pclsMst->byLogNum;
    vector<WORD> vecSlvAddr = pclsMst->vecSlvAddr;
    log_debug(byLogNum, "master_sendToSlaves().");

    WORD wSlvAddr = vecSlvAddr.front();
    /* 向主机主备线程接收端口发送数据 */
    dwRet = pMbufer->send_message(wSlvAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }
        

    return SUCCESS;
}


