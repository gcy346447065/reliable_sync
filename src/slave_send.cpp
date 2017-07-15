#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include "slave_send.h"
#include "macro.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

extern mbufer *g_pSlvMbufer;

static BYTE g_bySeq = 0;

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
    MSG_INFO_S stSendMsgInfo = {0, (DWORD)pbySendBuf, 0, 0};
    dwRet = g_pSlvMbufer->send_message(g_pSlvMbufer->g_byMstMsgAddr, stSendMsgInfo, wOffset);
    if (dwRet != SUCCESS)
    {
        log_error("send_message error!");
        return dwRet;
    }

    return dwRet;
}

VOID *slave_alloc_reqMsg(BYTE byCmd)
{
    WORD wMsgLen = 0;
    switch(byCmd)
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
        pstMsgHeader->wSig  = htons(START_FLAG);
        pstMsgHeader->bySeq = g_bySeq++;
        pstMsgHeader->byCmd = byCmd;
        pstMsgHeader->wLen  = htons(wMsgLen - MSG_HEADER_LEN);
    }

    return (VOID *)pstMsgHeader;
}