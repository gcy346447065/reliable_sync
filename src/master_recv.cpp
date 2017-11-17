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

DWORD master_recv(void *pMst, void *pRecvBuf, WORD *pwBufLen)
{
    DWORD dwRet = SUCCESS;

    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    mbufer *pclsMbufer = (mbufer *)pclsMst->pMbufer;

    /* 从mbufer中接收到消息体 */
    dwRet = pclsMbufer->receive_message(pRecvBuf, pwBufLen, DMM_NO_WAIT);
    if(dwRet != DMM_SUCCESS)
    {
        log_error(byLogNum, "receive_message error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD master_login(void *pMst, const void *pMsg)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    WORD wMstAddr = pclsMst->wMstAddr;
    mbufer *pMbufer = pclsMst->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
    log_debug(byLogNum, "master_login().");

    const MSG_LOGIN_REQ_S *pstReq = (const MSG_LOGIN_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_LOGIN_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_1)
    {
        log_debug(byLogNum, "START_SIG_1 login, task to master.");

        WORD wTaskAddr = ntohs(pstReq->stMsgHdr.wSrcAddr);
        if(pclsMst->wTaskAddr == 0)
        {
            pclsMst->wTaskAddr = wTaskAddr;

            MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(wMstAddr, wTaskAddr, 
                START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_LOGIN);
            if(!pstRsp)
            {
                log_error(byLogNum, "master_alloc_rspMsg error!");
                return FAILE;
            }
            pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;

            dwRet = master_sendToTask(pMst, (void *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "master_sendToOne error!");
            }
            free(pstRsp);
        }
    }
    else if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_2)
    {
        log_debug(byLogNum, "START_SIG_2 login, slave to master.");

        WORD wSlvAddr = ntohs(pstReq->stMsgHdr.wSrcAddr);
        log_debug(byLogNum, "wSlvAddr(%d).", wSlvAddr);
        //记录下wSlvAddr

        MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(wMstAddr, wSlvAddr, 
            START_SIG_2, ntohl(pstReq->stMsgHdr.dwSeq), CMD_LOGIN);
        if(!pstRsp)
        {
            log_error(byLogNum, "master_alloc_rspMsg error!");
            return FAILE;
        }
        pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;

        dwRet = master_sendToSlaves(pMst, (void *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendToOne error!");
        }
        free(pstRsp);
    }

    return dwRet;
}

static DWORD master_keepAlive(void *pMst, const void *pMsg)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_keepAlive().");
    DWORD dwRet = SUCCESS;

    const MSG_KEEP_ALIVE_RSP_S *pstRsp = (const MSG_KEEP_ALIVE_RSP_S *)pMsg;
    if(!pstRsp)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_KEEP_ALIVE_RSP_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg length not enough!");
        return FAILE;
    }
    BYTE wSlvAddr = pstRsp->stMsgHdr.wSrcAddr;
    log_debug(byLogNum, "wSlvAddr(%d).", wSlvAddr);

    //dwRet = g_mst_pSlvList->slv_resetKeepaliveSendTimes(wSlvAddr);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slv_resetKeepaliveSendTimes error!");
        return FAILE;
    }

    return SUCCESS;
}

static DWORD master_dataBatch(void *pMst, const void *pMsg)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    mbufer *pMbufer = pclsMst->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
    log_debug(byLogNum, "master_dataBatch().");

    const MSG_DATA_BATCH_REQ_S *pstReq = (const MSG_DATA_BATCH_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wSig) != START_SIG_1)
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_BATCH_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    //log_debug(byLogNum, "%4x, %4x, %4x, %4x", pstReq->stMsgHdr.wSig, pstReq->stMsgHdr.wVer, pstReq->stMsgHdr.wSrcAddr, pstReq->stMsgHdr.wDstAddr);
    //log_debug(byLogNum, "%8x, %4x, %4x", pstReq->stMsgHdr.dwSeq, pstReq->stMsgHdr.wCmd, pstReq->stMsgHdr.wLen);
    //log_debug(byLogNum, "%8x, %8x", pstReq->stData.dwDataStart, pstReq->stData.dwDataStart);
    //log_debug(byLogNum, "%8x, %4x, %4x", pstReq->stData.stData.dwDataID, pstReq->stData.stData.wDataLen, pstReq->stData.stData.wDataChecksum);

    MSG_DATA_BATCH_RSP_S *pstRsp = (MSG_DATA_BATCH_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_BATCH);
    pstRsp->stDataResult.dwDataID = pstReq->stData.stData.dwDataID;
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = master_sendToTask(pMst, (void *)pstRsp, sizeof(MSG_DATA_BATCH_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendToTask error!");
        return FAILE;
    }

    NODE_DATA_BATCH_S *pstNodeBatch = (NODE_DATA_BATCH_S *)malloc(sizeof(NODE_DATA_BATCH_S));
    pstNodeBatch->stState.byIsReady = FALSE;
    pstNodeBatch->stState.byIsSucceed = FALSE;
    pstNodeBatch->stState.bySendTimes = 0;
    memcpy(&(pstNodeBatch->stBatchNet), &(pstReq->stData), sizeof(DATA_BATCH_PKG_S));

    DWORD dwBatchID = ntohl(pstReq->stData.stData.dwDataID);
    pclsMst->dwBatchNow = dwBatchID;//每次Batch单包到达都通知procThread处理，万一不是所有单包都到达map，在slave接收端再作检测
    pclsMst->byBatchFlag = TRUE;
    pclsMst->mapDataBatch.insert(make_pair(dwBatchID, (void *)pstNodeBatch));

    free(pstRsp);
    return SUCCESS;
}

static DWORD master_dataInstant(void *pMst, const void *pMsg)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    mbufer *pMbufer = pclsMst->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
    log_debug(byLogNum, "master_dataInstant().");

    const MSG_DATA_INSTANT_REQ_S *pstReq = (const MSG_DATA_INSTANT_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wSig) != START_SIG_1)
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_INSTANT_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    MSG_DATA_INSTANT_RSP_S *pstRsp = (MSG_DATA_INSTANT_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_INSTANT);
    pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = master_sendToTask(pMst, (void *)pstRsp, sizeof(MSG_DATA_INSTANT_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendToTask error!");
        return FAILE;
    }

    NODE_DATA_INSTANT_S *pstNodeInstant = (NODE_DATA_INSTANT_S *)malloc(sizeof(NODE_DATA_INSTANT_S));
    pstNodeInstant->stState.byIsReady = FALSE;
    pstNodeInstant->stState.byIsSucceed = FALSE;
    pstNodeInstant->stState.bySendTimes = 0;
    memcpy(&(pstNodeInstant->stInstantNet), &(pstReq->stData), sizeof(DATA_PKG_S));

    DWORD dwInstantID = ntohl(pstReq->stData.dwDataID);
    pclsMst->dwInstantNow = dwInstantID;
    pclsMst->byInstantFlag = TRUE;
    pclsMst->mapDataInstant.insert(make_pair(dwInstantID, (void *)pstNodeInstant));
    
    free(pstRsp);
    return SUCCESS;
}

static DWORD master_dataWaited(void *pMst, const void *pMsg)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    mbufer *pMbufer = pclsMst->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
    log_debug(byLogNum, "master_dataWaited().");

    const MSG_DATA_WAITED_REQ_S *pstReq = (const MSG_DATA_WAITED_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wSig) != START_SIG_1)
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_WAITED_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    MSG_DATA_WAITED_RSP_S *pstRsp = (MSG_DATA_WAITED_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_WAITED);
    pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = master_sendToTask(pMst, (void *)pstRsp, sizeof(MSG_DATA_WAITED_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendToTask error!");
        return FAILE;
    }

    NODE_DATA_WAITED_S *pstNodeWaited = (NODE_DATA_WAITED_S *)malloc(sizeof(NODE_DATA_WAITED_S));
    pstNodeWaited->stState.byIsReady = FALSE;
    pstNodeWaited->stState.byIsSucceed = FALSE;
    pstNodeWaited->stState.bySendTimes = 0;
    memcpy(&(pstNodeWaited->stWaitedNet), &(pstReq->stData), sizeof(DATA_PKG_S));

    DWORD dwWaitedID = ntohl(pstReq->stData.dwDataID);
    pclsMst->dwWaitedNow = dwWaitedID;
    pclsMst->byInstantFlag = TRUE;
    pclsMst->mapDataWaited.insert(make_pair(dwWaitedID, (void *)pstNodeWaited));

    free(pstRsp);
    return SUCCESS;
}

static DWORD master_getDataCount(void *pMst, const void *pMsg)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    mbufer *pMbufer = pclsMst->pMbufer;
    if(!pMbufer)
    {
        log_error(byLogNum, "pMbufer error!");
        return FAILE;
    }
    log_debug(byLogNum, "master_getDataCount().");

    const MSG_GET_DATA_COUNT_REQ_S *pstReq = (const MSG_GET_DATA_COUNT_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wSig) != START_SIG_1)
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_GET_DATA_COUNT_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    DWORD dwBatchCount = 0, dwInstantCount = 0, dwWaitedCount = 0;
    dwBatchCount = pclsMst->mapDataBatch.size();
    dwInstantCount = pclsMst->mapDataInstant.size();
    dwWaitedCount = pclsMst->mapDataWaited.size();

    MSG_GET_DATA_COUNT_RSP_S *pstRsp = (MSG_GET_DATA_COUNT_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_GET_DATA_COUNT);
    pstRsp->dwBatchCount = htonl(dwBatchCount);
    pstRsp->dwInstantCount = htonl(dwInstantCount);
    pstRsp->dwWaitedCount = htonl(dwWaitedCount);

    DWORD dwRet = master_sendToTask(pMst, (void *)pstRsp, sizeof(MSG_GET_DATA_COUNT_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendToTask error!");
        return FAILE;
    }

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
    {CMD_DATA_BATCH,        master_dataBatch},
    {CMD_DATA_INSTANT,      master_dataInstant},
    {CMD_DATA_WAITED,       master_dataWaited},
    {CMD_GET_DATA_COUNT,    master_getDataCount}
};

static DWORD master_msgHandleOne(void *pMst, const MSG_HDR_S *pstMsgHdr)
{
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_msgHandleOne().");
    
    for(UINT i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].wCmd == ntohs(pstMsgHdr->wCmd))
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
    master *pclsMst = (master *)pMst;
    BYTE wMstAddr = pclsMst->wMstAddr;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_msgHandle().");

    if(wMsgLen < MSG_HDR_LEN)
    {
        log_error(byLogNum, "message length error(%u<%lu)!", wMsgLen, MSG_HDR_LEN);
        return FAILE;
    }

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

        if(ntohs(pstMsgHdr->wDstAddr) != (WORD)wMstAddr)
        {
            log_error(byLogNum, "wDstAddr(%u) not equal to wMstAddr(%u)", ntohs(pstMsgHdr->wDstAddr), wMstAddr);
            continue;
        }
            
        master_msgHandleOne(pMst, pstMsgHdr);//ntohs(pwSig[0])：START_SIG_1时为主机业务线程下发数据的消息，START_SIG_2时为备机主备线程回复的消息
        pwSig = (const WORD *)pbyMsg;
        if(wLeftLen == 0)
        {
            break;
        }
    }
    
    return SUCCESS;
}

