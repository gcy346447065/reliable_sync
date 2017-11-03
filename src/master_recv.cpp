#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <unistd.h> //for read STDIN_FILENO
#include <netinet/in.h> //for htons
#include "master.h"
#include "master_recv.h"
#include "master_send.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

//extern DWORD dwMasterHEHE;

void *master_allocRecvBuffer(WORD wBufLen)
{
    void *pRecvBuf = malloc(wBufLen);
    if(pRecvBuf == NULL)
    {
        return NULL;
    }
    memset(pRecvBuf, 0, wBufLen);
    return pRecvBuf;
}

DWORD master_freeRecvBuffer(void *pRecvBuf)
{
    free(pRecvBuf);
    return SUCCESS;
}

DWORD master_recv(void *pMst, void *pRecvBuf, WORD *pwBufLen)
{
    DWORD dwRet = SUCCESS;

    master *pclsMst = (master *)pMst;
    mbufer *pclsMbufer = (mbufer *)pclsMst->pMbufer;

    /* 从mbufer中接收到消息体 */
    dwRet = pclsMbufer->receive_message(pRecvBuf, pwBufLen, DMM_NO_WAIT);
    if(dwRet != DMM_SUCCESS)
    {
        log_error("receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD master_login(void *pMst, const void *pMsg)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_login.");

    master *pclsMst = (master *)pMst;
    BYTE byMstAddr = pclsMst->byMstAddr;

    const MSG_LOGIN_REQ_S *pstReq = (const MSG_LOGIN_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error("msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_LOGIN_REQ_S) - MSG_HDR_LEN)
    {
        log_error("msg length not enough!");
        return FAILE;
    }
    BYTE bySlvAddr = pstReq->stMsgHdr.wSrcAddr;
    log_debug("bySlvAddr(%d).", bySlvAddr);
    
    MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(byMstAddr, bySlvAddr, pstReq->stMsgHdr.dwSeq, CMD_LOGIN);
    if(!pstRsp)
    {
        log_error("master_alloc_rspMsg error!");
        return FAILE;
    }

    //dwRet = g_mst_pSlvList->slv_insert(bySlvAddr);
    if(dwRet == FAILE)
    {
        pstRsp->byLoginResult = LOGIN_RESULT_ERROR;
        log_info("This bySlvAddr(%d) register failed.", bySlvAddr);
    }
    else if(dwRet == SLV_HAS_REGED)
    {
        pstRsp->byLoginResult = LOGIN_RESULT_REGED;
        log_info("This bySlvAddr(%d) has registered.", bySlvAddr);
    }
    else if(dwRet == SUCCESS)
    {
        pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;
        log_info("This bySlvAddr(%d) is registering.", bySlvAddr);
    }

    //dwRet = master_sendToOne(bySlvAddr, (BYTE *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error("master_sendToOne error!");
        return FAILE;
    }
    
    free(pstRsp);
    return SUCCESS;
}

static DWORD master_keepAlive(void *pMst, const void *pMsg)
{
    log_debug("master_keepAlive.");
    DWORD dwRet = SUCCESS;

    const MSG_KEEP_ALIVE_RSP_S *pstRsp = (const MSG_KEEP_ALIVE_RSP_S *)pMsg;
    if(!pstRsp)
    {
        log_error("msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_KEEP_ALIVE_RSP_S) - MSG_HDR_LEN)
    {
        log_error("msg length not enough!");
        return FAILE;
    }
    BYTE bySlvAddr = pstRsp->stMsgHdr.wSrcAddr;
    log_debug("bySlvAddr(%d).", bySlvAddr);

    //dwRet = g_mst_pSlvList->slv_resetKeepaliveSendTimes(bySlvAddr);
    if(dwRet != SUCCESS)
    {
        log_error("slv_resetKeepaliveSendTimes error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD master_dataInstant(void *pMst, const void *pMsg)
{
    log_debug("master_dataInstant.");
    return SUCCESS;
}

static DWORD master_dataWaited(void *pMst, const void *pMsg)
{
    log_debug("master_dataWaited.");
    return SUCCESS;
}

typedef DWORD (*MSG_PROC)(void *pMst, const void *pMsg);
typedef struct
{
    WORD wCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             master_login},
    {CMD_KEEP_ALIVE,        master_keepAlive},
    {CMD_DATA_INSTANT,      master_dataInstant},
    {CMD_DATA_WAITED,       master_dataWaited}
};

static DWORD master_MsgHandleOne(void *pMst, const MSG_HDR_S *pstMsgHdr)
{
    //log_debug("master_MsgHandleOne.");
    for(UINT i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].wCmd == pstMsgHdr->wCmd)
        {
            MSG_PROC pfn = g_msgProcs[i].pfn;
            if(pfn)
            {
                return pfn(pMst, (const void *)pstMsgHdr);
            }
        }
    }

    return FAILE;//未解析出函数说明异常
}

DWORD master_msgHandle(void *pMst, const void *pMsg, WORD wMsgLen)
{
    if(wMsgLen < MSG_HDR_LEN)
    {
        log_error("message length error(%u<%lu)!", wMsgLen, MSG_HDR_LEN);
        return FAILE;
    }

    master *pclsMst = (master *)pMst;
    BYTE byMstAddr = pclsMst->byMstAddr;

    const BYTE *pbyMsg = (const BYTE *)pMsg;
    const WORD *pwSig = (const WORD *)pbyMsg;
    WORD wLeftLen = wMsgLen;
    while(ntohs(pwSig[0]) == START_SIG_1 || ntohs(pwSig[0]) == START_SIG_2)//为了处理粘包
    {
        const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pbyMsg;
        if(wLeftLen < sizeof(MSG_HDR_S))
        {
            break;
        }
        wLeftLen = wLeftLen - sizeof(MSG_HDR_S) - ntohs(pstMsgHdr->wLen);
        pbyMsg = pbyMsg + wMsgLen - wLeftLen;

        if(ntohs(pstMsgHdr->wDstAddr) != (WORD)byMstAddr)
        {
            log_error("wDstAddr(%u) not equal to byMstAddr(%u)", ntohs(pstMsgHdr->wDstAddr), byMstAddr);
            continue;
        }
            
        master_MsgHandleOne(pMst, pstMsgHdr);//ntohs(pwSig[0])：START_SIG_1时为主机业务线程下发数据的消息，START_SIG_2时为备机主备线程回复的消息
        pwSig = (const WORD *)pbyMsg;
    }
    
    return SUCCESS;
}

