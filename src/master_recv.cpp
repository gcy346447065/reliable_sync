#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <unistd.h> //for read STDIN_FILENO
#include <netinet/in.h> //for htons
#include "checksum.h"
#include "macro.h"
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

    if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_1) //收到task向master发送的login报文
    {
        log_debug(byLogNum, "START_SIG_1 login, task to master or slave.");

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

            dwRet = master_sendMsg(pMst, pclsMst->wTaskAddr, (void *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "master_sendToOne error!");
            }
            free(pstRsp);
        }
    }
    else if(ntohs(pstReq->stMsgHdr.wSig) == START_SIG_2) // 收到slave向master发送的login报文
    {
        log_debug(byLogNum, "START_SIG_2 login, slave to master.");

        WORD wSlvAddr = ntohs(pstReq->stMsgHdr.wSrcAddr);
        log_debug(byLogNum, "wSlvAddr(%d).", wSlvAddr);

        // 查找slave是否已经登录过了
        SLAVE_S stSlv;
        unsigned int uiSlvIdx;
        for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
        {
            stSlv = pclsMst->vecSlvs[uiSlvIdx];
            if(stSlv.wSlvAddr == wSlvAddr)
            {
                log_debug(byLogNum, "Slave has already logged in!");
                break;
            }
        }

        //记录下wSlvAddr
        if(uiSlvIdx == pclsMst->vecSlvs.size())
        {
            SLAVE_S stSlv;
            stSlv.wSlvAddr = wSlvAddr;
            stSlv.stBatch.byBatchFlag = FALSE;
            stSlv.stBatch.bySendtimes = 0;
            stSlv.stBatch.dwDataNums = 0;
            stSlv.stBatch.vecDataIDs.clear();
            
            stSlv.stInstant.byInstantFlag = FALSE;
            stSlv.stInstant.bySendtimes = 0;
            
            stSlv.stWaited.byWaitedFlag = FALSE;
            stSlv.stWaited.bySendtimes = 0;
            
            pclsMst->vecSlvs.push_back(stSlv);
            pclsMst->bySlvNums++;

            log_debug(byLogNum, "Slave(%d) has a successful login!", wSlvAddr);

            MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)master_alloc_rspMsg(wMstAddr, wSlvAddr, 
                START_SIG_2, ntohl(pstReq->stMsgHdr.dwSeq), CMD_LOGIN);
            if(!pstRsp)
            {
                log_error(byLogNum, "master_alloc_rspMsg error!");
                return FAILE;
            }
            pstRsp->byLoginResult = LOGIN_RESULT_SUCCEED;

            dwRet = master_sendMsg(pMst, wSlvAddr, (void *)pstRsp, sizeof(MSG_LOGIN_RSP_S));
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "master_sendMsg error!");
            }
            free(pstRsp);
        }
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

static DWORD master_batchRes2Tsk(void *pMst, const MSG_DATA_BATCH_REQ_S *pstReq)
{
    master *pclsMst = (master *)pMst;
    BYTE wMstAddr = pclsMst->wMstAddr;
    BYTE byLogNum = pclsMst->byLogNum;

    //log_debug(byLogNum, "master response batch backup to task.");

    // 构造batch回复报文
    MSG_DATA_BATCH_RSP_S *pstRsp = (MSG_DATA_BATCH_RSP_S *)master_alloc_rspMsg(wMstAddr, 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_BATCH);
    pstRsp->stDataResult.dwDataID = pstReq->stData.stData.dwDataID; //直接转发的字段，就不再转序
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = master_sendMsg(pMst, pclsMst->wTaskAddr, (void *)pstRsp, sizeof(MSG_DATA_BATCH_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendMsg error!");
        return FAILE;
    }
    free(pstRsp);

    // 将batch包插入到master的batch映射表里
    NODE_DATA_BATCH_S *pstNodeBatch = (NODE_DATA_BATCH_S *)malloc(sizeof(NODE_DATA_BATCH_S) + ntohs(pstReq->stData.stData.wDataLen));
    pstNodeBatch->stBatchNet.dwDataStart = ntohl(pstReq->stData.dwDataStart);
    pstNodeBatch->stBatchNet.dwDataEnd = ntohl(pstReq->stData.dwDataEnd);
    pstNodeBatch->stBatchNet.stData.dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    pstNodeBatch->stBatchNet.stData.wDataChecksum = ntohs(pstReq->stData.stData.wDataChecksum);
    pstNodeBatch->stBatchNet.stData.wDataLen = ntohs(pstReq->stData.stData.wDataLen);
    memcpy(&(pstNodeBatch->stBatchNet.stData.abyData), &(pstReq->stData.stData.abyData), ntohs(pstReq->stData.stData.wDataLen));

    pclsMst->mapDataBatch.insert(make_pair(ntohl(pstReq->stData.stData.dwDataID), pstNodeBatch));

    // 将batch包转发给所有slave
    DWORD dwDataStart = ntohl(pstReq->stData.dwDataStart);
    DWORD dwDataEnd = ntohl(pstReq->stData.dwDataEnd);
    DWORD dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    WORD wDataLen = ntohs(pstReq->stData.stData.wDataLen);

    MSG_DATA_BATCH_REQ_S *pstBatchPkg;
    SLAVE_S stSlv;
    UINT uiSlvIdx;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        stSlv = pclsMst->vecSlvs[uiSlvIdx];
        
        //设置slave的接收batch标志位
        if(dwDataID == dwDataStart)
        {
            stSlv.stBatch.byBatchFlag = TRUE;
            stSlv.stBatch.bySendtimes++;
        }
        if(dwDataID % 500 == 0)    //模拟丢包
        {
            log_debug(byLogNum, "batch pkgs(%u) failed to send", dwDataID);
            continue;
        }
        
        pstBatchPkg = (MSG_DATA_BATCH_REQ_S *)master_alloc_dataBatch(pclsMst->wMstAddr, stSlv.wSlvAddr, dwDataStart, dwDataEnd,
                                                      dwDataID, (void *)pstReq->stData.stData.abyData, wDataLen);
        
        DWORD dwRet = master_sendMsg((void *)pclsMst, stSlv.wSlvAddr, (void *)pstBatchPkg, sizeof(MSG_DATA_BATCH_REQ_S) + wDataLen);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendMsg error!");
            return FAILE;
        }

        free(pstBatchPkg);
    }

    return SUCCESS;
}

static DWORD master_batchRes2Slv(void *pMst, const MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp)
{
    master *pclsMst = (master *)pMst;
    BYTE wMstAddr = pclsMst->wMstAddr;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_batchRes2Slv().");
    
    WORD wSlvAddr = ntohs(pstRsp->stMsgHdr.wSrcAddr);
    DWORD dwPkgNums = ntohl(pstRsp->stSlvRecvResult.dwNeedPkgNums);
    DWORD dwDataStart = htonl(pstRsp->stSlvRecvResult.dwDataStart);
    DWORD dwDataEnd = htonl(pstRsp->stSlvRecvResult.dwDataEnd);
    
    log_debug(byLogNum, "wSlvAddr = %u, dwDataStart = %u, dwDataEnd = %u, dwPkgNums = %u.", wSlvAddr, dwDataStart, dwDataEnd, dwPkgNums);
    
    SLAVE_S *pstSlv;
    unsigned int uiSlvIdx;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        pstSlv = (SLAVE_S *)&(pclsMst->vecSlvs.at(uiSlvIdx));
        if(pstSlv->wSlvAddr == wSlvAddr)
        {
            log_debug(byLogNum, "Slave(%u) need batch pkgs!", ntohs(pstRsp->stMsgHdr.wSrcAddr));
            if(!dwPkgNums)
            {
                //该slave已batch完毕
                log_debug(byLogNum, "Slave(%d) has finished batch bakcup!", ntohs(pstRsp->stMsgHdr.wSrcAddr));
                
                pstSlv->stBatch.byBatchFlag = FALSE;
                pstSlv->stBatch.bySendtimes = 0;
                pstSlv->stBatch.dwDataNums = 0;
                pstSlv->stBatch.vecDataIDs.clear();
                
            }
            else if(dwPkgNums != pstSlv->stBatch.dwDataNums)
            {
                //这种情况属于master重传后slave依然有丢包现象，但不是所有包都丢了
                pstSlv->stBatch.bySendtimes = 1;
            }
            else
            {
                log_debug(byLogNum, "Slave(%u) need %u batch pkgs!", ntohs(pstRsp->stMsgHdr.wSrcAddr), dwPkgNums);
            }
            break;
        }
    }
        
    
    DWORD dwDataID;
    for(DWORD i = 0; i < dwPkgNums; i++)
    {
        dwDataID = ntohl(pstRsp->stSlvRecvResult.dwDataIDs[i]);
        log_debug(byLogNum, "Slave(%d) need pkg(%u, %u to %u)!", ntohs(pstRsp->stMsgHdr.wSrcAddr), dwDataID, dwDataStart, dwDataEnd);
        
        NODE_DATA_BATCH_S *pstNodeBatch = pclsMst->mapDataBatch[dwDataID];
        MSG_DATA_BATCH_REQ_S *pstBatchPkg = (MSG_DATA_BATCH_REQ_S *)master_alloc_dataBatch(wMstAddr, wSlvAddr, dwDataStart, dwDataEnd,
                                                          dwDataID, (void *)pstNodeBatch->stBatchNet.stData.abyData, pstNodeBatch->stBatchNet.stData.wDataLen);

        master_sendMsg((void *)pclsMst, wSlvAddr, (void *)pstBatchPkg, sizeof(MSG_DATA_BATCH_REQ_S) + pstNodeBatch->stBatchNet.stData.wDataLen);
        free(pstBatchPkg);
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
    //log_debug(byLogNum, "master_dataBatch().");

    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pMsg;
    if(!pstMsgHdr)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }

    // 解析报文
    if(ntohs(pstMsgHdr->wSig) == START_SIG_1)  //收到task下发的batch报文
    {
        const MSG_DATA_BATCH_REQ_S *pstReq = (const MSG_DATA_BATCH_REQ_S *)pMsg;
        if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_BATCH_REQ_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }

        // 回复task
        DWORD dwRet = master_batchRes2Tsk(pMst, pstReq);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master response batch backup to task failed!");
            return FAILE;
        }
    }
    else if(ntohs(pstMsgHdr->wSig) == START_SIG_2)  //收到某slave发来的batch反馈包
    {
        const MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (const MSG_DATA_SLAVE_BATCH_RSP_S *)pMsg;
        if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }
        
        // 回复slave
        DWORD dwRet = master_batchRes2Slv(pMst, pstRsp);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master response batch backup to slave failed!");
            return FAILE;
        }
    }
    else
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }

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

    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pMsg;
    if(!pstMsgHdr)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    
    if(ntohs(pstMsgHdr->wSig) == START_SIG_1)
    {
        //收到task下发的instant包，先回复task
        const MSG_DATA_INSTANT_REQ_S *pstReq = (const MSG_DATA_INSTANT_REQ_S *)pMsg;
        if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_INSTANT_REQ_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }
        
        MSG_DATA_INSTANT_RSP_S *pstRsp = (MSG_DATA_INSTANT_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
            ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_INSTANT);
        pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
        pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;
    
        DWORD dwRet = master_sendMsg(pMst, pclsMst->wTaskAddr, (void *)pstRsp, sizeof(MSG_DATA_INSTANT_RSP_S));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendMsg error!");
            return FAILE;
        }
    
        // 将Instant包插入到master的Instant映射表里
        NODE_DATA_INSTANT_S *pstNodeInstant = (NODE_DATA_INSTANT_S *)malloc(sizeof(NODE_DATA_INSTANT_S));
        memcpy(&(pstNodeInstant->stInstantNet), &(pstReq->stData), sizeof(DATA_PKG_S));
    
        //WORD wChecksum = checksum((const void *)(pstNodeInstant->stInstantNet.abyData), (WORD)ntohs(pstNodeInstant->stInstantNet.wDataLen));
        //pstNodeInstant->stInstantNet.wDataChecksum = htons(wChecksum);
    
        DWORD dwInstantID = ntohl(pstReq->stData.dwDataID);
        pclsMst->mapDataInstant.insert(make_pair(dwInstantID, pstNodeInstant));
    
        // 将instant包转发给所有slave
        log_debug(byLogNum, "being ready, send node.");
    
        SLAVE_S stSlv;
        MSG_DATA_INSTANT_REQ_S *pstInstantPkg;
        UINT uiSlvIdx;
        for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
        {
            stSlv = pclsMst->vecSlvs[uiSlvIdx];
            pstInstantPkg = (MSG_DATA_INSTANT_REQ_S *)master_alloc_dataInstant(pclsMst->wMstAddr, stSlv.wSlvAddr, dwInstantID,
                                                          (void *)pstReq->stData.abyData, ntohs(pstReq->stData.wDataLen));
            
            master_sendMsg((void *)pclsMst, stSlv.wSlvAddr, (void *)pstInstantPkg, sizeof(MSG_DATA_INSTANT_REQ_S) + ntohs(pstReq->stData.wDataLen));
    
            free(pstInstantPkg);
        }
        free(pstRsp);
    }
    else if(ntohs(pstMsgHdr->wSig) == START_SIG_2)
    {
        //收到某个slave发送回来的instant回复包
        const MSG_DATA_INSTANT_RSP_S *pstRsp = (const MSG_DATA_INSTANT_RSP_S *)pMsg;
        if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_DATA_INSTANT_RSP_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }

        //将存储在master里面的对应包byRecvTimes加一
        //NODE_DATA_INSTANT_S *pstNodeInstant = pclsMst->mapDataInstant[ntohl(pstRsp->stDataResult.dwDataID)];
        
        SLAVE_S stSlv;
        UINT uiSlvIdx;
        for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
        {
            stSlv = pclsMst->vecSlvs[uiSlvIdx];
            if(ntohs(pstRsp->stMsgHdr.wSrcAddr) == stSlv.wSlvAddr)
            {
                log_debug(byLogNum, "Slave(%d) has received instant pkg!", stSlv.wSlvAddr);
            }
        }
    }
    else
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }
    
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

    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pMsg;
    if(!pstMsgHdr)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    
    if(ntohs(pstMsgHdr->wSig) == START_SIG_1)
    {
        //收到task下发的waited包，先回复task
        const MSG_DATA_WAITED_REQ_S *pstReq = (const MSG_DATA_WAITED_REQ_S *)pMsg;
        if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_WAITED_REQ_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }
        
        MSG_DATA_WAITED_RSP_S *pstRsp = (MSG_DATA_WAITED_RSP_S *)master_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
            ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_1, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_WAITED);
        pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
        pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;
    
        DWORD dwRet = master_sendMsg(pMst, pclsMst->wTaskAddr, (void *)pstRsp, sizeof(MSG_DATA_WAITED_RSP_S));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendMsg error!");
            return FAILE;
        }
    
        // 将waited包插入到master的waited链表里
        NODE_DATA_WAITED_S *pstNodeWaited = (NODE_DATA_WAITED_S *)malloc(sizeof(NODE_DATA_WAITED_S));
        memcpy(&(pstNodeWaited->stWaitedNet), &(pstReq->stData), sizeof(DATA_PKG_S));

        //WORD wChecksum = checksum((const void *)(pstNodeInstant->stInstantNet.abyData), (WORD)ntohs(pstNodeInstant->stInstantNet.wDataLen));
        //pstNodeInstant->stInstantNet.wDataChecksum = htons(wChecksum);
    
        DWORD dwWaitedID = ntohl(pstReq->stData.dwDataID);
        pclsMst->mapDataWaited.insert(make_pair(dwWaitedID, pstNodeWaited));
    
        // 将waited包转发给所有slave
        log_debug(byLogNum, "being ready, send node.");
    
        SLAVE_S stSlv;
        MSG_DATA_WAITED_REQ_S *pstWaitedPkg;
        UINT uiSlvIdx;
        for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
        {
            stSlv = pclsMst->vecSlvs[uiSlvIdx];
            pstWaitedPkg = (MSG_DATA_WAITED_REQ_S *)master_alloc_dataWaited(pclsMst->wMstAddr, stSlv.wSlvAddr, dwWaitedID,
                                                          (void *)pstReq->stData.abyData, ntohs(pstReq->stData.wDataLen));
            
            master_sendMsg((void *)pclsMst, stSlv.wSlvAddr, (void *)pstWaitedPkg, sizeof(MSG_DATA_WAITED_REQ_S) + ntohs(pstReq->stData.wDataLen));

            free(pstWaitedPkg);
        }
        free(pstRsp);
    }
    else if(ntohs(pstMsgHdr->wSig) == START_SIG_2)
    {
        //收到某个slave发送回来的waited回复包
        const MSG_DATA_WAITED_RSP_S *pstRsp = (const MSG_DATA_WAITED_RSP_S *)pMsg;
        if(ntohs(pstRsp->stMsgHdr.wLen) < sizeof(MSG_DATA_WAITED_RSP_S) - MSG_HDR_LEN)
        {
            log_error(byLogNum, "msg wLen not enough!");
            return FAILE;
        }

        //将对应slave的waited标志位复位
        SLAVE_S stSlv;
        UINT uiSlvIdx;
        for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
        {
            stSlv = pclsMst->vecSlvs[uiSlvIdx];
            if(ntohs(pstRsp->stMsgHdr.wSrcAddr) == stSlv.wSlvAddr)
            {
                log_debug(byLogNum, "Slave(%d) has received waited pkg!", stSlv.wSlvAddr);
            }
        }
    }
    else
    {
        log_error(byLogNum, "msg wSig error!");
        return FAILE;
    }

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

    DWORD dwRet = master_sendMsg(pMst, pclsMst->wTaskAddr, (void *)pstRsp, sizeof(MSG_GET_DATA_COUNT_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendMsg error!");
        return FAILE;
    }

    return SUCCESS;
}

static MSG_PROC_MAP g_msgProcs_mst[] =
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
    //master *pclsMst = (master *)pMst;
    //BYTE byLogNum = pclsMst->byLogNum;
    //log_debug(byLogNum, "master_msgHandleOne().");
    
    for(UINT i = 0; i < sizeof(g_msgProcs_mst) / sizeof(g_msgProcs_mst[0]); i++)
    {
        if(g_msgProcs_mst[i].wCmd == ntohs(pstMsgHdr->wCmd))
        {
            MSG_PROC pfn = g_msgProcs_mst[i].pfn;
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
    //log_debug(byLogNum, "master_msgHandle().");

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
        if(wLeftLen <= 0)
        {
            break;
        }
    }
    
    return SUCCESS;
}

