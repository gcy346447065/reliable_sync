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
#include "log.h"

extern WORD g_slv_wMstAddr;
extern WORD g_slv_wSlvAddr;
extern mbufer *g_pSlvMbufer;
extern timer *g_pSlvRegTimer;

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

DWORD slave_batchWriteFile(void *pSlv, const MSG_DATA_BATCH_REQ_S *pstReq)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave write batch file.");

    //slave记录已收到的batch包
    DWORD dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    DWORD dwDataStart = pclsSlv->stBatch.dwDataStart;
    DWORD dwDataEnd = pclsSlv->stBatch.dwDataEnd;
    BYTE byOffset = 0x80; //0x80 = 1000 0000
    for(INT i = dwDataID; (i - dwDataStart) % 8 != 0; i--)
    {
        byOffset >>= 1;
    }
    pclsSlv->stBatch.pbyBitmap[(dwDataID - dwDataStart) / 8] |= byOffset;

    //slave将收到的batch包写入文件
    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFileName, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFileName, "batch_%u_to_%u", dwDataStart, dwDataEnd);
    INT iFileFd;
    if ((iFileFd = open(pcFileName, O_RDWR | O_CREAT, 0666)) < 0) {
        log_error(byLogNum, "create %s error!", pcFileName);
        return FAILE;
    }

    INT iFilePos = dwDataID - dwDataStart;
    lseek(iFileFd, iFilePos * MAX_TASK2MST_PKG_LEN, SEEK_SET);//定位到文件对应位置以写入包
    
    WORD wDataLen = ntohs(pstReq->stData.stData.wDataLen);
    INT iWriteLen = write(iFileFd, pstReq->stData.stData.abyData, wDataLen);
    if (iWriteLen == wDataLen) {
        //log_debug(byLogNum, "write batch file %s succeed.", pcFileName);
    } else {
        log_error(byLogNum, "write batch file %s error!", pcFileName);
    }

    free(pcFileName);
    close(iFileFd);

    return SUCCESS;
}

DWORD slave_batchCountLost(void *pSlv)
{
    slave *pclsSlv = (slave *)pSlv;
    //BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave count lost batch pkgs.");

    pclsSlv->stBatch.dwDataNums = 0;
    DWORD dwRet = 0;
    DWORD dwDataStart = pclsSlv->stBatch.dwDataStart;
    DWORD dwDataEnd = pclsSlv->stBatch.dwDataEnd;
    for(DWORD i = 0; i < (dwDataEnd - dwDataStart) / 8 + 1; i++)
    {
        if(pclsSlv->stBatch.pbyBitmap[i] != 0xff)
        {
            BYTE byOffset = 0x80; //0x80 = 1000 0000
            for(DWORD j = 0; j < 8; j++)
            {
                if(i * 8 + j + dwDataStart > dwDataEnd)
                {
                    break;
                }
                else if(!(pclsSlv->stBatch.pbyBitmap[i] & byOffset))
                {
                    pclsSlv->stBatch.dwDataNums++;
                    pclsSlv->stBatch.vecDataIDs.push_back(i * 8 + j + dwDataStart);
                }

                byOffset >>= 1;
            }
        }
    }

    dwRet = pclsSlv->stBatch.dwDataNums;
    
    //log_debug(byLogNum, "slave lose %u pkgs!", dwRet);
    
    return dwRet;
}

static DWORD slave_batchRes2Mst(void *pSlv)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave response batch result to master.");

    if(pclsSlv->stBatch.byBatchFlag != TRUE)
    {
        log_debug(byLogNum, "slave is not in batch state!");
        return SUCCESS;
    }

    // slave统计未收到的batch包
    DWORD dwNeedPkg = slave_batchCountLost(pSlv);
    if(dwNeedPkg == 0)
    {
        log_debug(byLogNum, "slave has received all batch pkg!");

        pclsSlv->stBatch.byBatchFlag = FALSE;
        pclsSlv->stBatch.dwDataNums = 0;
        pclsSlv->stBatch.bySendtimes = 0;
        pclsSlv->stBatch.vecDataIDs.clear();
        //pclsSlv->stBatch.dwDataStart = 0; //Start和End保留，以使slave能分辨多余的batch包
        //pclsSlv->stBatch.dwDataEnd = 0;
        free(pclsSlv->stBatch.pbyBitmap);
        
        //关batch定时器
        {

        }
    }
    else
    {
        log_debug(byLogNum, "slave still has %u pkgs to receive.", dwNeedPkg);
    }
    
    // 向master发送batch反馈报文
    MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (MSG_DATA_SLAVE_BATCH_RSP_S *)slave_alloc_batchRspMsg(pclsSlv->wSlvAddr, pclsSlv->wMstAddr, 0, 
                                                                                                dwNeedPkg, pclsSlv->stBatch.dwDataStart, pclsSlv->stBatch.dwDataEnd);
    if(dwNeedPkg != 0)
    {
        for(DWORD i = 0; i < dwNeedPkg; i++)
        {
            pstRsp->stSlvRecvResult.dwDataIDs[i] = htonl(pclsSlv->stBatch.vecDataIDs.at(i));
        }
    }
    
    DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) + dwNeedPkg * sizeof(DWORD));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_sendMsg error!");
        return FAILE;
    }

    log_debug(byLogNum, "slave response batch result to master!");
    
    free(pstRsp);

    return dwRet;
}

static DWORD slave_dataBatch(void *pSlv, const void *pMsg)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    BYTE byBatchFlag = FALSE;   //用于区分重传的标记位
    //log_debug(byLogNum, "slave_dataBatch.");
    
    const MSG_DATA_BATCH_REQ_S *pstReq = (const MSG_DATA_BATCH_REQ_S *)pMsg;
    if(ntohs(pstReq->stMsgHdr.wLen) < sizeof(MSG_DATA_BATCH_REQ_S) - MSG_HDR_LEN)
    {
        log_error(byLogNum, "msg wLen not enough!");
        return FAILE;
    }

    // slave进入batch状态
    DWORD dwDataID = ntohl(pstReq->stData.stData.dwDataID);
    DWORD dwDataStart = ntohl(pstReq->stData.dwDataStart);
    DWORD dwDataEnd = ntohl(pstReq->stData.dwDataEnd);
    if(pclsSlv->stBatch.byBatchFlag == FALSE)
    {
        if((dwDataStart == pclsSlv->stBatch.dwDataStart) && (dwDataEnd == pclsSlv->stBatch.dwDataEnd) && dwDataEnd)
        {
            //防止slave已经batch完又收到多余的batch包而重新进入batch状态
            MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (MSG_DATA_SLAVE_BATCH_RSP_S *)slave_alloc_batchRspMsg(pclsSlv->wSlvAddr, pclsSlv->wMstAddr, htonl(pstReq->stMsgHdr.dwSeq), 
                                                                                                        0, pclsSlv->stBatch.dwDataStart, pclsSlv->stBatch.dwDataEnd);
            
            DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_SLAVE_BATCH_RSP_S));
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "master_sendMsg error!");
                free(pstRsp);
                return FAILE;
            }
            
            free(pstRsp);
            return SUCCESS;
        }
        
        pclsSlv->stBatch.byBatchFlag = TRUE;
        pclsSlv->stBatch.bySendtimes = 0;
        pclsSlv->stBatch.dwDataNums = 0;
        pclsSlv->stBatch.vecDataIDs.clear();
        pclsSlv->stBatch.dwDataStart = dwDataStart;
        pclsSlv->stBatch.dwDataEnd = dwDataEnd;
        pclsSlv->stBatch.pbyBitmap = (BYTE *)malloc((dwDataEnd - dwDataStart) * sizeof(BYTE) / 8 + 1);
        memset(pclsSlv->stBatch.pbyBitmap, 0, (dwDataEnd - dwDataStart) * sizeof(BYTE) / 8 + 1);
        
        //开batch定时器
        {

        }
        
        log_debug(byLogNum, "slave start to batch!(%u to %u)", dwDataStart, dwDataEnd);
    }

    // slave写batch文件
    DWORD dwRet = slave_batchWriteFile(pSlv, pstReq);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave failed to write file!");
        return FAILE;
    }

    // 此时如果是第一次收到的batch包，则还没有进入过slave_batchCountLost函数，dwDataNums应该为0
    if(pclsSlv->stBatch.dwDataNums != 0)
    {
        //否则接收的是master重传过来的包
        DWORD dwNeedPkg = slave_batchCountLost(pSlv);
        if(!dwNeedPkg)
        {   //重传完毕
            byBatchFlag = TRUE;
        }
    }

    if(dwDataID == dwDataEnd || byBatchFlag)
    {    
        // slave统计并向master回复反馈包 
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

    //需要做一个更详细的长度检验
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

