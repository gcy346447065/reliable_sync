#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <netinet/in.h> //for htons
#include <stdio.h> // for sprintf
#include <fcntl.h> // for open
#include <unistd.h> // for write
#include <errno.h> //for errno
#include <sys/io.h> // for access
#include "macro.h"
#include "slave.h"
#include "slave_recv.h"
#include "slave_send.h"
#include "protocol.h"
#include "mbufer.h"
#include "timer.h"
#include "checksum.h"
#include "log.h"

extern WORD g_wByteBitCnt;

DWORD g_dwSlvLastDataNums;

DWORD slave_recv(void *pSlv, void *pRecvBuf, WORD *pwBufLen) 
{
    DWORD dwRet = SUCCESS;

    slave *pclsSlve = (slave *)pSlv;
    BYTE byLogNum = pclsSlve->byLogNum;
    mbufer *pclsMbufer = (mbufer *)pclsSlve->pMbufer;

    /* 从mbufer中接收到消息体 */
    dwRet = pclsMbufer->receive_message(pRecvBuf, pwBufLen, DMM_NO_WAIT);
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

    /*定时login功能先不管*/
	/*if(pstRsp->byLoginResult == LOGIN_RESULT_SUCCEED)
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
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;

    const MSG_KEEP_ALIVE_REQ_S *pstReq = (const MSG_KEEP_ALIVE_REQ_S *)pMsg;
    if(!pstReq)
    {
        log_error(byLogNum, "msg handle empty!");
        return FAILE;
    }
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_KEEP_ALIVE_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg length not enough!");
        return FAILE;
    }
    
    log_debug(byLogNum, "slave receive master(%u)'s keep alive msg.", ntohs(pstReq->stMsgHdr.wSrcAddr));

/*
    WORD wDstAddr = ntohs(pstReq->stMsgHdr.wSrcAddr);
    MSG_KEEP_ALIVE_RSP_S *pstRsp = (MSG_KEEP_ALIVE_RSP_S *)slave_alloc_rspMsg(pclsSlv->wSlvAddr, wDstAddr, 
                                                                              ntohs(pstReq->stMsgHdr.wSig), ntohl(pstReq->stMsgHdr.dwSeq), CMD_KEEP_ALIVE);
    if(!pstRsp)
    {
        log_error(byLogNum, "slave_alloc_rspMsg error!");
        return FAILE;
    }

    DWORD dwRet = slave_sendMsg(pSlv, wDstAddr, (void *)pstRsp, sizeof(MSG_KEEP_ALIVE_RSP_S) + pstRsp->stMsgHdr.wLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave_sendMsg error!");
        return FAILE;
    }
    free(pstRsp);
*/
    return SUCCESS;
}

DWORD slave_batchSetState(void *pSlv, BYTE byStartFlag, DWORD dwDataStart, DWORD dwDataEnd)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    
    if(byStartFlag)
    {
        log_debug(byLogNum, "slave start batching.");
        
        g_dwSlvLastDataNums = 0;
        
        pclsSlv->stBatch.byBatchFlag = TRUE;
        pclsSlv->stBatch.bySendtimes = 0;
        
        pclsSlv->stBatch.dwDataNums = 0;
        pclsSlv->stBatch.vecDataIDs.clear();
        
        pclsSlv->stBatch.dwDataStart = dwDataStart;
        pclsSlv->stBatch.dwDataEnd = dwDataEnd;
        pclsSlv->stBatch.pbyBitmap = (BYTE *)malloc(((dwDataEnd - dwDataStart) / g_wByteBitCnt + 1) * sizeof(BYTE));
        memset(pclsSlv->stBatch.pbyBitmap, 0, (((dwDataEnd - dwDataStart) / g_wByteBitCnt + 1) * sizeof(BYTE)));
        
        //开batch定时器
        DWORD dwRet = pclsSlv->pBatchTimer->start(NEWCFG_BATCH_TIMER_VALUE);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "pclsSlv->pBatchTimer start error!");
            return FAILE;
        }
    }
    else
    {
        log_debug(byLogNum, "slave end batching.");
        
        g_dwSlvLastDataNums = 0;
        
        pclsSlv->stBatch.byBatchFlag = FALSE;
        pclsSlv->stBatch.bySendtimes = 0;

        //不重置start和end是为了用于判断并防止重复收到该次的batch包
        
        pclsSlv->stBatch.dwDataNums = 0;
        pclsSlv->stBatch.vecDataIDs.clear();
        if(!pclsSlv->stBatch.pbyBitmap)
        {
            free(pclsSlv->stBatch.pbyBitmap);
        }
        
        //关batch定时器
        DWORD dwRet = pclsSlv->pBatchTimer->stop();
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "pclsSlv->pBatchTimer stop error!");
            return FAILE;
        }

    }

    return SUCCESS;
}

DWORD slave_batchWriteFile(void *pSlv, const MSG_DATA_BATCH_REQ_S *pstReq)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave write batch file.");
    
    WORD wDatachecksum = checksum((const void *)pstReq->stData.stData.abyData, ntohs(pstReq->stData.stData.wDataLen));
    if(wDatachecksum != ntohs(pstReq->stData.stData.wDataChecksum))
    {
        log_debug(byLogNum, "checksum error!");
        return FAILE;
    }
    
    DWORD dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    DWORD dwDataStart = pclsSlv->stBatch.dwDataStart;
    DWORD dwDataEnd = pclsSlv->stBatch.dwDataEnd;
    if(dwDataID > dwDataEnd)
    {
        log_debug(byLogNum, "dwDataID(%u) > dwDataEnd(%u).", dwDataID, dwDataEnd);
        return FAILE;
    }
    
    //slave记录已收到的batch包，其中g_wByteBitCnt是1 BYTE的位数
    BYTE byOffset = 0x01;
    for(INT i = 0; i < g_wByteBitCnt - 1; i++)
    {
        byOffset <<= 1;
    }
    for(INT i = dwDataID; (i - dwDataStart) % g_wByteBitCnt != 0; i--)
    {
        byOffset >>= 1;
    }
    pclsSlv->stBatch.pbyBitmap[(dwDataID - dwDataStart) / g_wByteBitCnt] |= byOffset;


    if(dwDataID == dwDataStart)
    {
        log_debug(byLogNum, "Batch file's name is %s.", pstReq->stData.stData.abyData);
        return SUCCESS;
    }

    //batch包写入文件
    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    INT iFileFd;
    memset(pcFileName, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFileName, "batch_%u_to_%u", dwDataStart, dwDataEnd);
    if ((iFileFd = open(pcFileName, O_RDWR | O_CREAT, 0666)) < 0) {
        log_error(byLogNum, "create %s error!", pcFileName);
        return FAILE;
    }
    
    INT iFilePos = dwDataID - dwDataStart - 1;
    lseek(iFileFd, iFilePos * MAX_TASK2MST_PKG_LEN, SEEK_SET);
    WORD wDataLen = ntohs(pstReq->stData.stData.wDataLen);
    INT iWriteLen = write(iFileFd, pstReq->stData.stData.abyData, wDataLen);
    if (iWriteLen != wDataLen) 
    {
        log_error(byLogNum, "write batch file %s error!", pcFileName);
    } 

    close(iFileFd);
    free(pcFileName);

    return SUCCESS;
}

DWORD slave_batchCountLost(void *pSlv)
{
    slave *pclsSlv = (slave *)pSlv;
    //BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave count lost batch pkgs.");

    DWORD dwDataStart = pclsSlv->stBatch.dwDataStart;
    DWORD dwDataEnd = pclsSlv->stBatch.dwDataEnd;

    pclsSlv->stBatch.dwDataNums = 0;
    pclsSlv->stBatch.vecDataIDs.clear();

    BYTE byByteSet = 0x01;
    BYTE byMax = 0x01;
    for(INT i = 0; i < g_wByteBitCnt - 1; i++)
    {
        byByteSet <<= 1;
        byMax |= byByteSet;
    }
    for(DWORD i = 0; i < (dwDataEnd - dwDataStart) / g_wByteBitCnt + 1; i++)
    {
        if(pclsSlv->stBatch.pbyBitmap[i] != byMax)
        {
            BYTE byOffset = byByteSet;
            for(DWORD j = 0; j < g_wByteBitCnt; j++)
            {
                if(i * g_wByteBitCnt + j + dwDataStart > dwDataEnd)
                {
                    break;
                }
                else if(!(pclsSlv->stBatch.pbyBitmap[i] & byOffset))
                {
                    pclsSlv->stBatch.dwDataNums++;
                    pclsSlv->stBatch.vecDataIDs.push_back(i * g_wByteBitCnt + j + dwDataStart);
                }

                byOffset >>= 1;
            }
        }
    }

    //log_debug(byLogNum, "slave lose %u pkgs!", dwRet);
    
    return pclsSlv->stBatch.dwDataNums;
}

DWORD slave_batchRes2Mst(void *pSlv)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave response batch result to master.");

    // slave统计未收到的batch包
    DWORD dwNeedPkg = slave_batchCountLost(pSlv);
    if(dwNeedPkg == 0)
    {
        log_debug(byLogNum, "slave has received all batch pkg!");

        DWORD dwRet = slave_batchSetState(pSlv, FALSE, 0, 0);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "slave end batch failed!");
            return FAILE;
        }
    }
    else
    {
        log_debug(byLogNum, "slave still has %u pkgs to receive.", dwNeedPkg);
    }
     
    // 向master发送batch反馈报文
    WORD wLoopFlag = TRUE;
    DWORD dwLoopTimes = 0;
    DWORD dwWriteNums = 0;
    while(dwNeedPkg >= 0)       //丢包个数较多是分片传输batch回复报文
    {
        if(dwNeedPkg >= MAX_SLAVE_RES_BATCH_PKGS)
        { 
            dwWriteNums = MAX_SLAVE_RES_BATCH_PKGS;
            dwNeedPkg -= MAX_SLAVE_RES_BATCH_PKGS;
        }
        else
        {
            dwWriteNums = dwNeedPkg;
            wLoopFlag = FALSE;
        }
                    
        MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (MSG_DATA_SLAVE_BATCH_RSP_S *)slave_alloc_dataBatchRsp(pclsSlv->wSlvAddr, pclsSlv->wMstAddr, 0, 
                                                                                                dwWriteNums, pclsSlv->stBatch.dwDataStart, pclsSlv->stBatch.dwDataEnd);
    
        for(DWORD i = 0; i < dwWriteNums; i++)
        {
            //log_debug(byLogNum, "dwNeedPkg = %u, pclsSlv->stBatch.vecDataIDs.size() = %d, i = %u, index = %u.", dwNeedPkg, (WORD)pclsSlv->stBatch.vecDataIDs.size(), i, dwLoopTimes * MAX_SLAVE_RES_BATCH_PKGS + i);
            pstRsp->stSlvRecvResult.dwDataIDs[i] = htonl(pclsSlv->stBatch.vecDataIDs.at(dwLoopTimes * MAX_SLAVE_RES_BATCH_PKGS + i));
        }
        
        DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) + dwWriteNums * sizeof(DWORD));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendMsg error!");
            return FAILE;
        }

        //log_debug(byLogNum, "slave response batch result to master!");
        free(pstRsp);
        
        dwLoopTimes++;
        if(!wLoopFlag)
        {
            break;
        }
    }
    
    return SUCCESS;
}

DWORD slave_dataBatch(void *pSlv, const void *pMsg)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave_dataBatch.");
    
    const MSG_DATA_BATCH_REQ_S *pstReq = (const MSG_DATA_BATCH_REQ_S *)pMsg;
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_BATCH_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    DWORD dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    DWORD dwDataStart = ntohl(pstReq->stData.dwDataStart);
    DWORD dwDataEnd = ntohl(pstReq->stData.dwDataEnd);
    if(pclsSlv->stBatch.byBatchFlag == FALSE)
    {
        if((dwDataStart == pclsSlv->stBatch.dwDataStart) && (dwDataEnd == pclsSlv->stBatch.dwDataEnd) && dwDataEnd)
        {
            //防止slave已经batch完又收到多余的batch包而重新进入batch状态
            //log_debug(byLogNum, "slave has already batched!(%u to %u)", dwDataStart, dwDataEnd);
            return SUCCESS;
        }
        else
        {
            DWORD dwRet = slave_batchSetState(pSlv, TRUE, dwDataStart, dwDataEnd);
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "slave start batching failed!(%u to %u)", dwDataStart, dwDataEnd);
                return FAILE;
            }
        }
    }

    DWORD dwRet = slave_batchWriteFile(pSlv, pstReq);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave failed to write file!");
        return FAILE;
    }

    // 此时如果是第一次收到的batch包，dwDataNums应该为0;否则接收的是master重传过来的包
    if(pclsSlv->stBatch.dwDataNums != 0)
    {
        if(g_dwSlvLastDataNums < pclsSlv->stBatch.dwDataNums - 1)
        {
            g_dwSlvLastDataNums++;
        }
        else
        {
            g_dwSlvLastDataNums = 0;
            DWORD dwRet = slave_batchRes2Mst(pSlv);
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "slave failed to response batch to master!");
                return FAILE;
            }
        }
    }

    if(dwDataID == dwDataEnd)
    {    
        DWORD dwRet = slave_batchRes2Mst(pSlv);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "slave failed to response batch to master!");
            return FAILE;
        }
    }

    return SUCCESS;
}

static DWORD slave_dataInstant(void *pSlv, const void *pMsg)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_dataInstant.");

    const MSG_DATA_INSTANT_REQ_S *pstReq = (const MSG_DATA_INSTANT_REQ_S *)pMsg;
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_INSTANT_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    WORD wDatachecksum = checksum((const void *)pstReq->stData.abyData, ntohs(pstReq->stData.wDataLen));
    if(wDatachecksum != ntohs(pstReq->stData.wDataChecksum))
    {
        log_debug(byLogNum, "checksum error!");
        return FAILE;
    }

    // 将instant报文存为文件
    DWORD dwDataID = ntohl(pstReq->stData.dwDataID);
    WORD wDataLen = ntohs(pstReq->stData.wDataLen);
    log_debug(byLogNum, "after ntohs: pstReq->stData.dwDataID(%u), pstReq->stData.wDataLen(%u)", dwDataID, wDataLen);

    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    sprintf(pcFileName, "instant_%u_%u", dwDataID, wDataLen);
    INT iFileFd;
    if ((iFileFd = open(pcFileName, O_RDWR | O_CREAT| O_TRUNC, 0666)) < 0) {
        log_error(byLogNum, "create %s error!", pcFileName);
        return FAILE;
    }

    INT iWriteLen = write(iFileFd, pstReq->stData.abyData, wDataLen);
    if (iWriteLen == wDataLen) {
        log_info(byLogNum, "write instant file %s succeed.", pcFileName);
    } else {
        log_error(byLogNum, "write instant file %s error!", pcFileName);
    }
    close(iFileFd);
    
    // 向master发送回复报文
    MSG_DATA_INSTANT_RSP_S *pstRsp = (MSG_DATA_INSTANT_RSP_S *)slave_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_2, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_INSTANT);
    pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_INSTANT_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendMsg error!");
        return FAILE;
    }

    free(pstRsp);
    return SUCCESS;
}

static DWORD slave_dataWaited(void *pSlv, const void *pMsg)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_dataWaited.");

    const MSG_DATA_WAITED_REQ_S *pstReq = (const MSG_DATA_WAITED_REQ_S *)pMsg;
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_WAITED_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    WORD wDatachecksum = checksum((const void *)pstReq->stData.abyData, ntohs(pstReq->stData.wDataLen));
    if(wDatachecksum != ntohs(pstReq->stData.wDataChecksum))
    {
        log_debug(byLogNum, "checksum error! checksum = %u.", wDatachecksum);
        return FAILE;
    }

    // 将waited报文存为文件
    DWORD dwDataID = ntohl(pstReq->stData.dwDataID);
    WORD wDataLen = ntohs(pstReq->stData.wDataLen);
    log_debug(byLogNum, "after ntohs: pstReq->stData.dwDataID(%u), pstReq->stData.wDataLen(%u)", dwDataID, wDataLen);

    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    sprintf(pcFileName, "waited_%u_%u", dwDataID, wDataLen);
    INT iFileFd;
    if ((iFileFd = open(pcFileName, O_RDWR | O_CREAT| O_TRUNC, 0666)) < 0) {
        log_error(byLogNum, "create %s error!", pcFileName);
        return FAILE;
    }

    INT iWriteLen = write(iFileFd, pstReq->stData.abyData, wDataLen);
    if (iWriteLen == wDataLen) {
        log_info(byLogNum, "write waited file %s succeed.", pcFileName);
    } else {
        log_error(byLogNum, "write waited file %s error!", pcFileName);
    }
    close(iFileFd);
    
    // 向master发送回复报文
    MSG_DATA_WAITED_RSP_S *pstRsp = (MSG_DATA_WAITED_RSP_S *)slave_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
        ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_2, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_WAITED);
    pstRsp->stDataResult.dwDataID = pstReq->stData.dwDataID;
    pstRsp->stDataResult.byResult = DATA_RESULT_SUCCEED;

    DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_WAITED_RSP_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendMsg error!");
        return FAILE;
    }

    free(pstRsp);
    return SUCCESS;
}

static MSG_PROC_MAP g_msgProcs_slv[] =
{
    {CMD_LOGIN,             slave_login},
    {CMD_KEEP_ALIVE,        slave_keepAlive},
    {CMD_DATA_BATCH,        slave_dataBatch},
    {CMD_DATA_INSTANT,      slave_dataInstant},
    {CMD_DATA_WAITED,       slave_dataWaited}
};

static DWORD slave_msgHandleOne(void *pSlv, const MSG_HDR_S *pstMsgHdr)
{
    //slave *pclsSlv = (slave *)pSlv;
    //BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave_msgHandleOne().");
	
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

DWORD slave_msgHandle(void *pSlv, const void *pMsg, WORD wMsgLen)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE wSlvAddr = pclsSlv->wSlvAddr;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave_msgHandle.");

    const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pMsg;
    if(wMsgLen < ntohs(pstMsgHdr->wLen) + sizeof(MSG_HDR_S))
    {
        log_error(byLogNum, "message length error!");
        return FAILE;
    }

    const BYTE *pbyMsg = (const BYTE *)pMsg;
    const WORD *pwSig = (const WORD *)pbyMsg;
    WORD wLeftLen = wMsgLen;
    while(ntohs(pwSig[0]) == START_SIG_1 || ntohs(pwSig[0]) == START_SIG_2)//为了处理粘包
    {
        const MSG_HDR_S *pstMsgHdr = (const MSG_HDR_S *)pbyMsg;
        
        //log_debug(byLogNum, "wMsgLen = %u, pstReq_Len = %u.", wMsgLen, ntohs(pstMsgHdr->wLen));
        
        if(wLeftLen < sizeof(MSG_HDR_S))
        {
            break;
        }
        wLeftLen = wLeftLen - sizeof(MSG_HDR_S) - ntohs(pstMsgHdr->wLen);
        pbyMsg = pbyMsg + wMsgLen - wLeftLen;

        if(ntohs(pstMsgHdr->wDstAddr) != (WORD)wSlvAddr)
        {
            log_error(byLogNum, "wDstAddr(%u) not equal to wSlvAddr(%u)", ntohs(pstMsgHdr->wDstAddr), wSlvAddr);
            continue;
        }

        slave_msgHandleOne(pSlv, pstMsgHdr);//ntohs(pwSig[0])：START_SIG_1时为主机业务线程下发数据的消息，START_SIG_2时为主机master回复的消息

        pwSig = (const WORD *)pbyMsg;
        if(wLeftLen <= 0)
        {
            break;
        }
    }

    return SUCCESS;
}

