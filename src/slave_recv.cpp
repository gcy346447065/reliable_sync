#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <netinet/in.h> //for htons
#include "macro.h"
#include "slave.h"
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

DWORD slave_recv(void *pSlv, BYTE *pbyRecvBuf, WORD *pwBufLen)
{
    DWORD dwRet = SUCCESS;

    slave *pclsSlve = (slave *)pSlv;
    BYTE byLogNum = pclsSlve->byLogNum;
    mbufer *pclsMbufer = (mbufer *)pclsSlve->pMbufer;

    /* 从mbufer中接收到消息体 */
    dwRet = pclsMbufer->receive_message(pbyRecvBuf, pwBufLen, DMM_NO_WAIT);
    if(dwRet != DMM_SUCCESS)
    {
        log_error(byLogNum, "receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD slave_login(void *pSlv, const void *pMsg)
{
    DWORD dwRet = FAILE;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    mbufer *pMbufer = pclsSlv->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
	
    log_debug(byLogNum, "slave_login.");

    const MSG_LOGIN_REQ_S *pstReq = (const MSG_LOGIN_REQ_S *)pMsg;

    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_LOGIN_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg length not enough!");
        return FAILE;
    }

    if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_1)
    {
        log_debug(byLogNum, "START_SIG_1 login, task to master or slave.");

        //Slave记录Task的地址
        pclsSlv->wTaskAddr = ntohs(pstReq->stMsgHdr.wSrcAddr);
        WORD wTaskAddr = pclsSlv->wTaskAddr;

        //收到登录成功回复包
        log_info(byLogNum, "This wSlvAddr(%d) logged in wTaskAddr(%d) success.", pclsSlv->wSlvAddr, pclsSlv->wTaskAddr);

        MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)slave_alloc_rspMsg(pclsSlv->wSlvAddr, wTaskAddr, 
                START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_LOGIN);
        if(!pstRsp)
        {
            log_error(byLogNum, "slave_alloc_rspMsg error!");
            return FAILE;
        }
        pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;

        dwRet = slave_sendMsg(pSlv, pclsSlv->wTaskAddr, (void *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "slave_sendMsg error!");
            return FAILE;
        }
        free(pstRsp);
    }

    else if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_2)
    {
        log_debug(byLogNum, "START_SIG_2 login, master response to slave.");

        //收到登录成功回复包
        log_info(byLogNum, "This wSlvAddr(%d) logged in wMstAddr(%d) success.", pclsSlv->wSlvAddr, pclsSlv->wMstAddr);

    }

/*定时login功能先不管
	if(pstRsp->byLoginResult == LOGIN_RESULT_SUCCEED)
    {
        //收到登录成功回复包
        log_info(byLogNum, "This wSlvAddr(%d) logged success.", pstRsp->stMsgHdr.wDstAddr);

        dwRet = g_pSlvRegTimer->stop();
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "g_pSlvRegTimer->stop error!");
            return FAILE;
        }
    }
*/
    return SUCCESS;
}

static DWORD slave_keepAlive(void *pSlv, const void *pMsg)
{
    log_debug(LOG1, "slave_keepAlive.");

    const MSG_KEEP_ALIVE_REQ_S *pstReq = (const MSG_KEEP_ALIVE_REQ_S *)pMsg;
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
    
    MSG_KEEP_ALIVE_RSP_S *pstRsp = (MSG_KEEP_ALIVE_RSP_S *)slave_alloc_rspMsg(pstReq->stMsgHdr.wSrcAddr, pstReq->stMsgHdr.wDstAddr, 
                                                                              pstReq->stMsgHdr.wSig, pstReq->stMsgHdr.dwSeq, CMD_KEEP_ALIVE);
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

static DWORD slave_dataInstant(void *pSlv, const void *pMsg)
{
    log_debug(LOG1, "slave_dataInstant.");
    return SUCCESS;
}

static DWORD slave_dataWaited(void *pSlv, const void *pMsg)
{
    log_debug(LOG1, "slave_dataWaited.");
    return SUCCESS;
}

static MSG_PROC_MAP g_msgProcs_slv[] =
{
    {CMD_LOGIN,             slave_login},
    {CMD_KEEP_ALIVE,        slave_keepAlive},
    {CMD_DATA_INSTANT,      slave_dataInstant},
    {CMD_DATA_WAITED,       slave_dataWaited}
};

static DWORD slave_msgHandleOne(void *pSlv, const MSG_HDR_S *pstMsgHdr)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_msgHandleOne().");
	
    for(UINT i = 0; i < sizeof(g_msgProcs_slv) / sizeof(g_msgProcs_slv[0]); i++)
    {
        if(g_msgProcs_slv[i].wCmd == ntohs(pstMsgHdr->wCmd))
        {
            MSG_PROC pfn = g_msgProcs_slv[i].pfn;
            if(pfn)
            {
                return pfn(pSlv, (const void *)pstMsgHdr);
            }
        }
    }

    return FAILE;//未解析出函数说明异常
}

DWORD slave_msgHandle(void *pSlv, const BYTE *pbyMsg, WORD wMsgLen)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
	
    log_debug(byLogNum, "slave_msgHandle.");
    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pbyMsg;

    if(wMsgLen < MSG_HDR_LEN)
    {
        log_error(byLogNum, "sync message length not enough(%u<%lu)", wMsgLen, MSG_HDR_LEN);
        return FAILE;
    }
	
/* 老代码不用
    if(pstMsgHdr->wSrcAddr != wMstAddr)
    {
        log_error(byLogNum, "wSrcAddr(%u) not equal to g_wMstAddr(%u)", pstMsgHdr->wSrcAddr, wMstAddr);
        return FAILE;
    }

    if(pstMsgHdr->wDstAddr != wSlvAddr)
    {
        log_error(byLogNum, "byDstAddr(%u) not equal to g_wSlvAddr(%u)", pstMsgHdr->wDstAddr, wSlvAddr);
        return FAILE;
    }
*/
    const WORD *pwSig = (const WORD *)pbyMsg;
    WORD wLeftLen = wMsgLen;
    while(wLeftLen >= ntohs(pstMsgHdr->wLen) + MSG_HDR_LEN)
    {
        if((ntohs(pwSig[0]) != START_SIG_1) && (ntohs(pwSig[0]) != START_SIG_2))
        {
            log_error(byLogNum, "signature error(%x)!", (unsigned)ntohs(pstMsgHdr->wSig));
            return FAILE;
        }

        slave_msgHandleOne(pSlv, pstMsgHdr);//如果多个数据包中有一个数据包未找到相应的解析函数时，暂未记录此异常情况
        wLeftLen = wLeftLen - MSG_HDR_LEN - ntohs(pstMsgHdr->wLen);
        pstMsgHdr = (const MSG_HDR_S *)(pbyMsg + wMsgLen - wLeftLen);
    }

    return SUCCESS;
}

