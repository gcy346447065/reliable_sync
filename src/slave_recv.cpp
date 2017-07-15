#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <netinet/in.h> //for htons
#include "slave_recv.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"

extern mbufer *g_pSlvMbufer;

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
        log_error("receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static WORD slave_login(const BYTE *pbyMsg)
{
    log_debug("slave_login.");
    return SUCCESS;
}

static WORD slave_keepAlive(const BYTE *pbyMsg)
{
    log_debug("slave_keepAlive.");
    return SUCCESS;
}

static WORD slave_newCfgInstant(const BYTE *pbyMsg)
{
    log_debug("slave_newCfgInstant.");
    return SUCCESS;
}

static WORD slave_newCfgWaited(const BYTE *pbyMsg)
{
    log_debug("slave_newCfgWaited.");
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
    {CMD_LOGIN,             slave_login},
    {CMD_KEEP_ALIVE,        slave_keepAlive},
    {CMD_NEWCFG_INSTANT,    slave_newCfgInstant},
    {CMD_NEWCFG_WAITED,     slave_newCfgWaited}
};

static DWORD slave_msgHandleOne(const BYTE *pbyMsg)
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

DWORD slave_msgHandle(const BYTE *pbyMsg, WORD wMsgLen)
{
    log_debug("slave_msgHandle.");
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

        slave_msgHandleOne((const BYTE *)pstMsgHeader);//如果多个数据包中有一个数据包未找到相应的解析函数时，暂未记录此异常情况
        wLeftLen = wLeftLen - MSG_HEADER_LEN - ntohs(pstMsgHeader->wLen);
        pstMsgHeader = (const MSG_HEADER_S *)(pbyMsg + wMsgLen - wLeftLen);
    }

    return SUCCESS;
}
