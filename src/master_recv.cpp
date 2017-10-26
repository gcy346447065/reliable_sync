#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <unistd.h> //for read STDIN_FILENO
#include <netinet/in.h> //for htons
#include "master_recv.h"
#include "master_send.h"
#include "protocol.h"
#include "mbufer.h"
#include "log.h"
#include "list_slv.h"
#include "list_data.h"

extern BYTE g_mst_byMstAddr;
extern mbufer *g_pMstMbufer;
extern list_slv *g_mst_pSlvList;
extern list_data *g_mst_pDataList;

//extern DWORD dwMasterHEHE;

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

static DWORD master_login(const BYTE *pbyMsg)
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
    BYTE bySlvAddr = pstReq->stMsgHeader.bySrcAddr;
    log_debug("bySlvAddr(%d).", bySlvAddr);
    
    MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(bySlvAddr, pstReq->stMsgHeader.wSeq, CMD_LOGIN);
    if(!pstRsp)
    {
        log_error("master_alloc_rspMsg error!");
        return FAILE;
    }

    dwRet = g_mst_pSlvList->slv_insert(bySlvAddr);
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

    dwRet = master_sendToOne(bySlvAddr, (BYTE *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error("master_sendToOne error!");
        return FAILE;
    }
    
    free(pstRsp);
    return SUCCESS;
}

static DWORD master_keepAlive(const BYTE *pbyMsg)
{
    log_debug("master_keepAlive.");

    const MSG_KEEP_ALIVE_RSP_S *pstRsp = (const MSG_KEEP_ALIVE_RSP_S *)pbyMsg;
    if(!pstRsp)
    {
        log_error("msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstRsp->stMsgHeader.wLen) < sizeof(MSG_KEEP_ALIVE_RSP_S) - MSG_HEADER_LEN)
    {
        log_error("msg length not enough!");
        return FAILE;
    }
    BYTE bySlvAddr = pstRsp->stMsgHeader.bySrcAddr;
    log_debug("bySlvAddr(%d).", bySlvAddr);

    DWORD dwRet = g_mst_pSlvList->slv_resetKeepaliveSendTimes(bySlvAddr);
    if(dwRet != SUCCESS)
    {
        log_error("slv_resetKeepaliveSendTimes error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD master_dataInstant(const BYTE *pbyMsg)
{
    log_debug("master_dataInstant.");
    return SUCCESS;
}

static DWORD master_dataWaited(const BYTE *pbyMsg)
{
    log_debug("master_dataWaited.");
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
    {CMD_LOGIN,             master_login},
    {CMD_KEEP_ALIVE,        master_keepAlive},
    {CMD_DATA_INSTANT,      master_dataInstant},
    {CMD_DATA_WAITED,       master_dataWaited}
};

static DWORD master_SyncMsgHandle(const MSG_HEADER_S *pstMsgHeader)
{
    //log_debug("master_SyncMsgHandle.");

    for(UINT i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].wCmd == pstMsgHeader->wCmd)
        {
            MSG_PROC pfn = g_msgProcs[i].pfn;
            if(pfn)
            {
                return pfn((const BYTE *)pstMsgHeader);
            }
        }
    }

    return FAILE;//未解析出函数说明异常
}

static DWORD master_IssueMsgHandle(const MSG_DATA_S *pstDataNET)
{
    //log_debug("master_IssueMsgHandle.");

    if(!pstDataNET)
    {
        log_error("pstDataNET empty!");
        return FAILE;
    }
    //dwMasterHEHE++;//用于test发来的数据包计数测试

    //log_debug("wDataSeq(%d), byDataType(%d), wDataLen(%d).", ntohs(pstDataNET->wDataSeq), pstDataNET->byDataType, ntohs(pstDataNET->wDataLen));
    //log_debug("wDataID(%d), wBatchStart(%d), wBatchEnd(%d).", ntohs(pstDataNET->wDataID), ntohs(pstDataNET->wBatchStart), ntohs(pstDataNET->wBatchEnd));

    DWORD dwRet = g_mst_pDataList->data_insert(pstDataNET);
    if(dwRet != SUCCESS)
    {
        log_error("data_insert error!");
        return FAILE;
    }

    /*INT iValue;
    socklen_t optlen;
    getsockopt(g_pMstMbufer->g_dwSocketFd, SOL_SOCKET, SO_RCVBUF, &iValue, &optlen);
    log_debug("SO_RCVBUF(%d)", iValue);*/

    //log_hex((const BYTE *)pstDataNET, 15);//WORD wDataID; WORD wBatchStart; WORD wBatchEnd;

    return SUCCESS;
}

DWORD master_msgHandle(const BYTE *pbyMsg, WORD wMsgLen)
{
    if(wMsgLen < MSG_HEADER_LEN && wMsgLen < sizeof(MSG_DATA_S))
    {
        log_error("message length error(%u<%luor%lu)!", wMsgLen, MSG_HEADER_LEN, sizeof(MSG_DATA_S));
        return FAILE;
    }

    const BYTE *pbyTmpMsg = pbyMsg;
    const WORD *pwSig = (const WORD *)pbyTmpMsg;
    WORD wLeftLen = wMsgLen;
    while(ntohs(pwSig[0]) == START_FLAG_1 || ntohs(pwSig[0]) == START_FLAG_2)
    {
        if(ntohs(pwSig[0]) == START_FLAG_1)
        {
            //log_debug("START_FLAG_1.");
            const MSG_HEADER_S *pstMsgHeader = (const MSG_HEADER_S *)pbyTmpMsg;
            if(wLeftLen < sizeof(MSG_HEADER_S))
            {
                break;
            }

            //log_debug("wLeftLen(%d).", wLeftLen);
            wLeftLen = wLeftLen - sizeof(MSG_HEADER_S) - ntohs(pstMsgHeader->wLen);
            pbyTmpMsg = pbyTmpMsg + wMsgLen - wLeftLen;
            //log_debug("wLeftLen(%d).", wLeftLen);
            if(pstMsgHeader->byDstAddr != g_mst_byMstAddr)
            {
                log_error("byDstAddr(%u) not equal to g_byMstAddr(%u)", pstMsgHeader->byDstAddr, g_mst_byMstAddr);
                continue;
            }

            master_SyncMsgHandle(pstMsgHeader);
        }
        else if(ntohs(pwSig[0]) == START_FLAG_2)
        {
            //log_debug("START_FLAG_2.");
            const MSG_DATA_S *pstDataNET = (const MSG_DATA_S *)pbyTmpMsg;
            if(wLeftLen < sizeof(MSG_DATA_S))
            {
                break;
            }

            //log_debug("wLeftLen(%d).", wLeftLen);
            wLeftLen = wLeftLen - sizeof(MSG_DATA_S) - ntohs(pstDataNET->wDataLen);
            //log_debug("wLeftLen(%d).", wLeftLen);
            pbyTmpMsg = pbyTmpMsg + wMsgLen - wLeftLen;

            master_IssueMsgHandle(pstDataNET);//业务流程发来的下发消息
        }

        pwSig = (const WORD *)pbyTmpMsg;
    }
    
    return SUCCESS;
}

/*
DWORD master_msgHandle(const BYTE *pbyMsg, WORD wMsgLen)
{
    log_debug("master_msgHandle.");
    const MSG_HEADER_S *pstMsgHeader = (const MSG_HEADER_S *)pbyMsg;

    if(wMsgLen < MSG_HEADER_LEN)
    {
        log_error("sync message length not enough(%u<%u)", wMsgLen, MSG_HEADER_LEN);
        return FAILE;
    }

    if(pstMsgHeader->byDstAddr != g_pMstMbufer->g_byMstAddr)
    {
        log_error("byDstAddr(%u) not equal to g_byMstAddr(%u)", pstMsgHeader->byDstAddr, g_pMstMbufer->g_byMstAddr);
        return FAILE;
    }

    WORD wLeftLen = wMsgLen;
    while(wLeftLen >= ntohs(pstMsgHeader->wLen) + MSG_HEADER_LEN)
    {
        const BYTE *pbyStatus = (const BYTE *)(&(pstMsgHeader->wSig));
        if((pbyStatus[0] != START_FLAG_1 / 0x100) || (pbyStatus[1] != START_FLAG_1 % 0x100))
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
*/
