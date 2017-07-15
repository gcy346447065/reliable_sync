#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <unistd.h> //for read STDIN_FILENO
#include <netinet/in.h> //for htons
#include "master_recv.h"
#include "master_send.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

extern mbufer *g_pMstMbufer;

BYTE *master_alloc_RecvBuffer(WORD wBufLen)
{
    BYTE *pbyRecvBuf = (BYTE *)malloc(wBufLen);
    if(pbyRecvBuf == NULL)
    {
        return NULL;
    }
    memset(pbyRecvBuf, 0, wBufLen);
    return pbyRecvBuf;
}

DWORD master_free(BYTE *pbyRecvBuf)
{
    free(pbyRecvBuf);
    return SUCCESS;
}

DWORD master_recv(BYTE *pbyRecvBuf, WORD *pwBufLen)
{
    DWORD dwRet = SUCCESS;

    /* 从mbufer中接收到消息体 */
    dwRet = g_pMstMbufer->receive_message(pbyRecvBuf, pwBufLen, DMM_NO_WAIT);
    if(dwRet != DMM_SUCCESS)
    {
        log_error("receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static WORD master_login(const BYTE *pbyMsg)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_login.");

    const MSG_LOGIN_REQ_S *pstReq = (const MSG_LOGIN_REQ_S *)pbyMsg;
    if(!pstReq)
    {
        log_error("msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHeader.wLen) < sizeof(MSG_LOGIN_REQ_S) - MSG_HEADER_LEN)
    {
        log_error("msg length not enough!");
        return FAILE;
    }

    log_debug("byMstAddr(%d), bySlvAddr(%d), byEndFlag(%d).", pstReq->byMstAddr, pstReq->bySlvAddr, pstReq->byEndFlag);
    
    if(pstReq->byEndFlag == LOGIN_END_FLAG_DISABLED)
    {
        //说明是收到的第一个登录包，可立即准备第二个登录结果包
        MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(CMD_LOGIN, pstReq->stMsgHeader.bySeq);
        if(!pstRsp)
        {
            log_error("master_alloc_rspMsg error!");
            return FAILE;
        }

        if(pstReq->byMstAddr == 0 || pstReq->bySlvAddr == 0)
        {
            log_info("byMstAddr or bySlvAddr is 0.");
            pstRsp->byLoginResult = LOGIN_RESULT_ERROR;
        }

        if(g_pMstMbufer->g_byMstMsgAddr == pstReq->byMstAddr)//主机地址正确
        {
            for(INT i = 0; i < sizeof(g_pMstMbufer->g_abySlvMsgAddrs); i++)//可能出现备机过多注册不上的情况
            {
                if(g_pMstMbufer->g_abySlvMsgAddrs[i] == pstReq->bySlvAddr)
                {
                    //说明这个备机已经登录过
                    log_info("This bySlvAddr(%d) has been logged.");
                    pstRsp->byLoginResult = LOGIN_RESULT_ERROR;
                }
                else if(g_pMstMbufer->g_abySlvMsgAddrs[i] == 0)
                {
                    //说明这个备机即将登录
                    g_pMstMbufer->g_abySlvMsgAddrs[i] = pstReq->bySlvAddr;

                    //返回第二个登录成功包
                    pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;
                }
            }
        }
        else
        {
            log_info("g_byMstMsgAddr(%d) != byMstAddr(%d).", g_pMstMbufer->g_byMstMsgAddr, pstReq->byMstAddr);
            pstRsp->byLoginResult = LOGIN_RESULT_ERROR;
        }

        dwRet = master_sendToOne(pstReq->bySlvAddr, (BYTE *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
        if(dwRet != SUCCESS)
        {
            log_error("master_sendToOne error!");
            return FAILE;
        }
    }
    else if(pstReq->byEndFlag == LOGIN_END_FLAG_ENABLED)
    {
        //说明是收到的第三个登录结束包
    }
    
    return SUCCESS;
}

static WORD master_keepAlive(const BYTE *pbyMsg)
{
    log_debug("master_keepAlive.");
    return SUCCESS;
}

static WORD master_newCfgInstant(const BYTE *pbyMsg)
{
    log_debug("master_newCfgInstant.");
    return SUCCESS;
}

static WORD master_newCfgWaited(const BYTE *pbyMsg)
{
    log_debug("master_newCfgWaited.");
    return SUCCESS;
}

typedef WORD (*MSG_PROC)(const BYTE *pbyMsg);
typedef struct
{
    BYTE byCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             master_login},
    {CMD_KEEP_ALIVE,        master_keepAlive},
    {CMD_NEWCFG_INSTANT,    master_newCfgInstant},
    {CMD_NEWCFG_WAITED,     master_newCfgWaited}
};

static DWORD master_msgHandleOne(const BYTE *pbyMsg)
{
    const MSG_HEADER_S *pstMsgHeader = (const MSG_HEADER_S *)pbyMsg;
    for(INT i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].byCmd == pstMsgHeader->byCmd)
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

DWORD master_msgHandle(const BYTE *pbyMsg, WORD wMsgLen)
{
    log_debug("master_MsgHandle.");
    const MSG_HEADER_S *pstMsgHeader = (const MSG_HEADER_S *)pbyMsg;

    if(wMsgLen < MSG_HEADER_LEN)
    {
        log_error("sync message length not enough(%u<%u)", wMsgLen, MSG_HEADER_LEN);
        return FAILE;
    }

    WORD wLeftLen = wMsgLen;
    while(wLeftLen >= ntohs(pstMsgHeader->wLen) + MSG_HEADER_LEN)
    {
        const BYTE *pbyStatus = (const BYTE *)(&(pstMsgHeader->wSig));
        if((pbyStatus[0] != START_FLAG / 0x100) || (pbyStatus[1] != START_FLAG % 0x100))
        {
            log_error("signature error(%x)!", (unsigned)ntohs(pstMsgHeader->wSig));
            return FAILE;
        }

        master_msgHandleOne((const BYTE *)pstMsgHeader);//如果多个数据包中有一个数据包未找到相应的解析函数时，暂未记录此异常情况
        wLeftLen = wLeftLen - MSG_HEADER_LEN - ntohs(pstMsgHeader->wLen);
        pstMsgHeader = (const MSG_HEADER_S *)(pbyMsg + wMsgLen - wLeftLen);
    }

    return SUCCESS;
}
