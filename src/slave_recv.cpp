#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <netinet/in.h> //for htons
#include "slave_recv.h"
#include "slave_send.h"
#include "protocol.h"
#include "mbufer.h"
#include "timer.h"
#include "log.h"

extern WORD g_slv_wMstAddr;
extern WORD g_slv_wSlvAddr;
extern mbufer *g_pSlvMbufer;
extern timer *g_pSlvRegTimer;

BYTE *slave_alloc_RecvBuffer(WORD wBufLen)
{
    BYTE *pbyRecvBuf = (BYTE *)malloc(wBufLen);
    if(pbyRecvBuf == NULL)
    {
        return NULL;
    }
    memset(pbyRecvBuf, 0, wBufLen);
    return pbyRecvBuf;
}

DWORD slave_free(BYTE *pbyRecvBuf)
{
    free(pbyRecvBuf);
    return SUCCESS;
}

DWORD slave_recv(BYTE *pbyRecvBuf, WORD *pwBufLen)
{
    DWORD dwRet = SUCCESS;

    /* 从mbufer中接收到消息体 */
    dwRet = g_pSlvMbufer->receive_message(pbyRecvBuf, pwBufLen, DMM_NO_WAIT);
    if(dwRet != DMM_SUCCESS)
    {
        log_error(LOG1, "receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD slave_login(const BYTE *pbyMsg)
{
    DWORD dwRet = SUCCESS;
    log_debug(LOG1, "slave_login.");

    const MSG_LOGIN_RSP_S *pstRsp = (const MSG_LOGIN_RSP_S *)pbyMsg;
    if(!pstRsp)
    {
        log_error(LOG1, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_LOGIN_RSP_S) - MSG_HDR_LEN)
    {
        log_error(LOG1, "msg length not enough!");
        return FAILE;
    }

    log_debug(LOG1, "wSlvAddr(%d), byLoginResult(%d).", pstRsp->stMsgHdr.wDstAddr, pstRsp->byLoginResult);
    if(pstRsp->byLoginResult == LOGIN_RESULT_SUCCEED)
    {
        //收到登录成功回复包
        log_info(LOG1, "This wSlvAddr(%d) logged success.", pstRsp->stMsgHdr.wDstAddr);

        dwRet = g_pSlvRegTimer->stop();
        if(dwRet != SUCCESS)
        {
            log_error(LOG1, "g_pSlvRegTimer->stop error!");
            return FAILE;
        }
    }

    return SUCCESS;
}

static DWORD slave_keepAlive(const BYTE *pbyMsg)
{
    log_debug(LOG1, "slave_keepAlive.");

    const MSG_KEEP_ALIVE_REQ_S *pstReq = (const MSG_KEEP_ALIVE_REQ_S *)pbyMsg;
    if(!pstReq)
    {
        log_error(LOG1, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_KEEP_ALIVE_REQ_S) - MSG_HDR_LEN)
    {
        log_error(LOG1, "msg length not enough!");
        return FAILE;
    }
    
    MSG_KEEP_ALIVE_RSP_S *pstRsp = (MSG_KEEP_ALIVE_RSP_S *)slave_alloc_rspMsg(pstReq->stMsgHdr.dwSeq, CMD_KEEP_ALIVE);
    if(!pstRsp)
    {
        log_error(LOG1, "slave_alloc_rspMsg error!");
        return FAILE;
    }

    DWORD dwRet = slave_send((BYTE *)pstRsp, sizeof(MSG_KEEP_ALIVE_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(LOG1, "slave_send error!");
        return FAILE;
    }
    free(pstRsp);

    return SUCCESS;
}

static DWORD slave_dataInstant(const BYTE *pbyMsg)
{
    log_debug(LOG1, "slave_dataInstant.");
    return SUCCESS;
}

static DWORD slave_dataWaited(const BYTE *pbyMsg)
{
    log_debug(LOG1, "slave_dataWaited.");
    return SUCCESS;
}

typedef DWORD (*MSG_PROC)(const BYTE *pbyMsg);
typedef struct
{
    WORD wCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             slave_login},
    {CMD_KEEP_ALIVE,        slave_keepAlive},
    {CMD_DATA_INSTANT,      slave_dataInstant},
    {CMD_DATA_WAITED,       slave_dataWaited}
};

static DWORD slave_msgHandleOne(const BYTE *pbyMsg)
{
    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pbyMsg;
    for(UINT i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].wCmd == pstMsgHdr->wCmd)
        {
            MSG_PROC pfn = g_msgProcs[i].pfn;
            if(pfn)
            {
                return pfn(pbyMsg);
            }
        }
    }

    return FAILE;//未解析出函数说明异常
}

DWORD slave_msgHandle(const BYTE *pbyMsg, WORD wMsgLen)
{
    log_debug(LOG1, "slave_msgHandle.");
    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pbyMsg;

    if(wMsgLen < MSG_HDR_LEN)
    {
        log_error(LOG1, "sync message length not enough(%u<%lu)", wMsgLen, MSG_HDR_LEN);
        return FAILE;
    }

    if(pstMsgHdr->wSrcAddr != g_slv_wMstAddr)
    {
        log_error(LOG1, "wSrcAddr(%u) not equal to g_wMstAddr(%u)", pstMsgHdr->wSrcAddr, g_slv_wMstAddr);
        return FAILE;
    }

    if(pstMsgHdr->wDstAddr != g_slv_wSlvAddr)
    {
        log_error(LOG1, "byDstAddr(%u) not equal to g_wSlvAddr(%u)", pstMsgHdr->wDstAddr, g_slv_wSlvAddr);
        return FAILE;
    }

    WORD wLeftLen = wMsgLen;
    while(wLeftLen >= ntohs(pstMsgHdr->wLen) + MSG_HDR_LEN)
    {
        const BYTE *pbyStatus = (const BYTE *)(&(pstMsgHdr->wSig));
        if((pbyStatus[0] != START_SIG_1 / 0x100) || (pbyStatus[1] != START_SIG_1 % 0x100))
        {
            log_error(LOG1, "signature error(%x)!", (unsigned)ntohs(pstMsgHdr->wSig));
            return FAILE;
        }

        slave_msgHandleOne((const BYTE *)pstMsgHdr);//如果多个数据包中有一个数据包未找到相应的解析函数时，暂未记录此异常情况
        wLeftLen = wLeftLen - MSG_HDR_LEN - ntohs(pstMsgHdr->wLen);
        pstMsgHdr = (const MSG_HDR_S *)(pbyMsg + wMsgLen - wLeftLen);
    }

    return SUCCESS;
}

