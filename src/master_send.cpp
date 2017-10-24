#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include "master_send.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

extern mbufer *g_pMstMbufer;
extern BYTE g_mst_byMstAddr;

static WORD g_wSeq = 0;

DWORD master_sendToOne(BYTE bySlvAddr, BYTE *pbyMsg, WORD wMsgLen)
{
    DWORD dwRet = SUCCESS;

    /* 申请mbufer发送消息体内存 */
    BYTE *pbySendBuf = NULL;
    dwRet = g_pMstMbufer->alloc_msg((VOID**)&pbySendBuf, (WORD)sizeof(CMD_S) + wMsgLen);
    if(dwRet != SUCCESS)
    {
        pbySendBuf = NULL;
        return dwRet;
    }
    dwRet = g_pMstMbufer->set_cmd_head_flag(pbySendBuf, CRM_HEADINFO_REQCMD);
    if(dwRet != SUCCESS)
    {
        g_pMstMbufer->free_msg(pbySendBuf);
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
    dwRet = g_pMstMbufer->add_to_packet(pbySendBuf, &stCmdHeader, &wOffset);//可能是对于同一个pbySendBuf上面可能有多条stCmdHeader
    if (dwRet != SUCCESS)
    {
        g_pMstMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /* 将mbufer发送消息体发送出去 */
    MSG_INFO_S stSendMsgInfo = {0, (DWORD)pbySendBuf, 0, 0};
    dwRet = g_pMstMbufer->send_message(bySlvAddr, stSendMsgInfo, wOffset);//对所有的slave执行发送
    if (dwRet != SUCCESS)
    {
        log_error("send_message error!");
        g_pMstMbufer->free_msg(pbySendBuf);
        return dwRet;
    }

    g_pMstMbufer->free_msg(pbySendBuf);
    return dwRet;
}

DWORD master_sendToMany(BYTE abySlvMsgAddrs[], BYTE *pbyMsg, WORD wMsgLen)
{
    DWORD dwRet = SUCCESS;

    /* 申请mbufer发送消息体内存 */
    BYTE *pbySendBuf = NULL;
    dwRet = g_pMstMbufer->alloc_msg((VOID**)&pbySendBuf, (WORD)sizeof(CMD_S) + wMsgLen);
    if(dwRet != SUCCESS)
    {
        pbySendBuf = NULL;
        return dwRet;
    }
    dwRet = g_pMstMbufer->set_cmd_head_flag(pbySendBuf, CRM_HEADINFO_REQCMD);
    if(dwRet != SUCCESS)
    {
        g_pMstMbufer->free_msg(pbySendBuf);
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
    dwRet = g_pMstMbufer->add_to_packet(pbySendBuf, &stCmdHeader, &wOffset);//可能是对于同一个pbySendBuf上面可能有多条stCmdHeader
    if (dwRet != SUCCESS)
    {
        g_pMstMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /* 将mbufer发送消息体发送出去 */
    MSG_INFO_S stSendMsgInfo = {0, (DWORD)pbySendBuf, 0, 0};
    for(UINT i = 0; i < sizeof(abySlvMsgAddrs); i++)//可能出现备机过多注册不上的情况
    {
        if(abySlvMsgAddrs[i] == 0)
        {
            break;
        }

        dwRet = g_pMstMbufer->send_message(abySlvMsgAddrs[i], stSendMsgInfo, wOffset);//对所有的slave执行发送
        if (dwRet != SUCCESS)
        {
            log_error("send_message error!");
            return dwRet;
        }
    }

    return dwRet;
}

VOID *master_alloc_reqMsg(BYTE bySlvAddr, WORD wCmd)
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

    MSG_HEADER_S *pstMsgHeader = (MSG_HEADER_S *)malloc(wMsgLen);
    if(pstMsgHeader)
    {
        pstMsgHeader->wSig  = htons(START_FLAG_1);
        pstMsgHeader->bySrcAddr = g_mst_byMstAddr;
        pstMsgHeader->byDstAddr = bySlvAddr;
        pstMsgHeader->wSeq = g_wSeq++;
        pstMsgHeader->wCmd = wCmd;
        pstMsgHeader->wLen  = htons(wMsgLen - MSG_HEADER_LEN);
    }

    return (VOID *)pstMsgHeader;
}

VOID *master_alloc_rspMsg(BYTE bySlvAddr, WORD wSeq, WORD wCmd)
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

        default:
            return NULL;
    }

    MSG_HEADER_S *pstMsgHeader = (MSG_HEADER_S *)malloc(wMsgLen);
    if(pstMsgHeader)
    {
        pstMsgHeader->wSig  = htons(START_FLAG_1);
        pstMsgHeader->bySrcAddr = g_mst_byMstAddr;
        pstMsgHeader->byDstAddr = bySlvAddr;
        pstMsgHeader->wSeq = wSeq;
        pstMsgHeader->wCmd = wCmd;
        pstMsgHeader->wLen  = htons(wMsgLen - MSG_HEADER_LEN);
    }

    return (VOID *)pstMsgHeader;
}

