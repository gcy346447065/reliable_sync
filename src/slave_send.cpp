#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include "slave.h"
#include "slave_send.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

extern WORD g_slv_wMstAddr;
extern WORD g_slv_wSlvAddr;
extern mbufer *g_pSlvMbufer;

static WORD g_dwSeq = 0;

DWORD slave_send(BYTE *pbyMsg, WORD wMsgLen)
{
    DWORD dwRet = SUCCESS;

    /* 申请mbufer发送消息体内存 */
    BYTE *pbySendBuf = NULL;
    dwRet = g_pSlvMbufer->alloc_msg((VOID**)&pbySendBuf, (WORD)sizeof(CMD_S) + wMsgLen);
    if(dwRet != SUCCESS)
    {
        pbySendBuf = NULL;
        return dwRet;
    }
    dwRet = g_pSlvMbufer->set_cmd_head_flag(pbySendBuf, CRM_HEADINFO_REQCMD);
    if(dwRet != SUCCESS)
    {
        g_pSlvMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /* 将主备模块消息体与命令头消息封入mbufer发送消息体，并返回dwOffset留待后用 */
    WORD wOffset = 0;
    CMD_S stCmdHeader;
    stCmdHeader.wCmdIdx = 0;
    stCmdHeader.wCtrlCmd = 0x4000;
    stCmdHeader.wCmd = 1;
    stCmdHeader.wParaLen = wMsgLen;
    stCmdHeader.pbyPara = pbyMsg;
    dwRet = g_pSlvMbufer->add_to_packet(pbySendBuf, &stCmdHeader, &wOffset);//可能是对于同一个pbySendBuf上面可能有多条stCmdHeader
    if (dwRet != SUCCESS)
    {
        g_pSlvMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /* 将mbufer发送消息体发送出去 */
    //MSG_INFO_S stSendMsgInfo = {0, (DWORD)pbySendBuf, 0, 0};
    dwRet = g_pSlvMbufer->send_message(g_slv_wMstAddr, pbySendBuf, wOffset);
    if (dwRet != SUCCESS)
    {
        log_error(LOG1, "send_message error!");
        g_pSlvMbufer->free_msg(pbySendBuf);
        return dwRet;
    }

    g_pSlvMbufer->free_msg(pbySendBuf);
    return dwRet;
}

VOID *slave_alloc_reqMsg(WORD wCmd)
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
        pstMsgHdr->wSrcAddr = g_slv_wSlvAddr;
        pstMsgHdr->wDstAddr = g_slv_wMstAddr;
        pstMsgHdr->dwSeq = g_dwSeq++;
        pstMsgHdr->wCmd = wCmd;
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
        pstMsgHdr->dwSeq = htons(dwSeq);
        pstMsgHdr->wCmd = htons(wCmd);
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

DWORD slave_sendMsg(void *pSlv, WORD wDstAddr, void *pData, WORD wDataLen)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    mbufer *pMbufer = pclsSlv->pMbufer;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_sendMsg().");

    /* 向指定地址发送数据 */
    dwRet = pMbufer->send_message(wDstAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }
        
    return SUCCESS;
}

VOID *slave_alloc_dataWaitedRspMsg(WORD dwSeq, WORD wDataQty)
{
    WORD wMsgLen = sizeof(MSG_DATA_INSTANT_RSP_S) + wDataQty * sizeof(DATA_RESULT_S);

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)malloc(wMsgLen);
    if(pstMsgHdr)
    {
        pstMsgHdr->wSig  = htons(START_SIG_1);
        pstMsgHdr->wSrcAddr = g_slv_wSlvAddr;
        pstMsgHdr->wDstAddr = g_slv_wMstAddr;
        pstMsgHdr->dwSeq = dwSeq;
        pstMsgHdr->wCmd = CMD_DATA_WAITED;
        pstMsgHdr->wLen  = htons(wMsgLen - MSG_HDR_LEN);
    }

    return (VOID *)pstMsgHdr;
}

