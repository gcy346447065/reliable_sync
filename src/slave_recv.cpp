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

DWORD slave_writeBatchPkgs(void *pSlv, const MSG_DATA_BATCH_REQ_S *pstReq)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_writeBatchPkgs().");

    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    CHAR *pcPreFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFileName, 0, MAX_STDIN_FILE_LEN);
    memset(pcPreFileName, 0, MAX_STDIN_FILE_LEN);
    
    sprintf(pcFileName, "batch_%u_to_%u_%u", ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd), ntohl(pstReq->stData.stData.dwDataID));
    sprintf(pcPreFileName, "batch_%u_to_%u_%u", ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd), ntohl(pstReq->stData.stData.dwDataID) - 1);

    INT iFileFd;
    if(access(pcPreFileName, 0))
    {
        //前一个batch包没收到，直接存储
        if((iFileFd = open(pcFileName, O_RDWR | O_CREAT | O_TRUNC, 0666)) < 0) 
        {
            log_error(byLogNum, "create %s error!", pcFileName);
            close(iFileFd);
            free(pcPreFileName);
            free(pcFileName);
            return FAILE;
        }
    }
    else
    {
        //否则写在前一个包文件的末端
        if((iFileFd = open(pcPreFileName, O_RDWR | O_CREAT | O_APPEND, 0666)) < 0) 
        {
            log_error(byLogNum, "write %s error!", pcPreFileName);
            close(iFileFd);
            free(pcPreFileName);
            free(pcFileName);
            return FAILE;
        }
    }
    
    WORD wDataLen = ntohs(pstReq->stData.stData.wDataLen);
    INT iWriteLen = write(iFileFd, pstReq->stData.stData.abyData, wDataLen);
    if (iWriteLen == wDataLen) {
        //log_debug(byLogNum, "write batch file %s succeed.", pcFileName);
    } else {
        log_error(byLogNum, "write batch file %s error!", pcFileName);
        close(iFileFd);
        free(pcPreFileName);
        free(pcFileName);
        return FAILE;
    }
    
    close(iFileFd);
    free(pcPreFileName);
    free(pcFileName);
    
    return SUCCESS;
}

DWORD slave_countLostPkgs(void *pSlv, DWORD dwDataStart, DWORD dwDataEnd)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_countLostPkgs.");

    DWORD dwRet = 0;
    if(pclsSlv->stBatch.byBatchFlag == TRUE)
    {
        for(DWORD dwDataID = dwDataStart; dwDataID <= dwDataEnd; dwDataID++)
        {
            CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
            sprintf(pcFileName, "batch_%u_to_%u_%u", dwDataStart, dwDataEnd, dwDataID);
            
            if(access(pcFileName, 0))
            {
                //指定dwDataID的文件不存在
                pclsSlv->stBatch.wDataNums++;
                pclsSlv->stBatch.vecDataIDs.push_back(dwDataID);

                dwRet++;
            }
            else
            {
                continue;
            }
        }
    }
    
    return dwRet;
}

DWORD slave_mergeBatchPkg(void *pSlv, DWORD dwDataStart, DWORD dwDataEnd)
{
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_mergeBatchPkg.");
    
    CHAR *pcBatchFile = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    sprintf(pcBatchFile, "batch_%u_to_%u", dwDataStart, dwDataEnd);
    INT iBatchFileFd;
    if ((iBatchFileFd = open(pcBatchFile, O_RDWR | O_CREAT| O_APPEND , 0666)) < 0) 
    {  
        log_error(byLogNum, "create %s error!", pcBatchFile);
        return FAILE;
    }
    
    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    void *pDataBuf = (void *)malloc(MAX_TASK2MST_PKG_LEN);
    for(DWORD dwDataID = dwDataStart; dwDataID <= dwDataEnd; dwDataID++)
    {
        memset(pcFileName, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFileName, "batch_%u_to_%u_%u", dwDataStart, dwDataEnd, dwDataID);
        
        INT iFileFd;
        if((iFileFd = open(pcFileName, O_RDONLY)) < 0)
        {
            log_error(byLogNum, "open new config file(%s) error(%s)!", pcFileName, strerror(errno));
            free(pDataBuf);
            free(pcFileName);
            free(pcBatchFile);
            return FAILE;
        }

        INT iFileLen = lseek(iFileFd, 0, SEEK_END);//定位到文件尾以得到文件大小
        lseek(iFileFd, 0, SEEK_SET);//重新定位到文件头

        memset(pDataBuf, 0, MAX_TASK2MST_PKG_LEN);
        
        INT iFileBufLen = read(iFileFd, pDataBuf, iFileLen);
        if(iFileBufLen < 0)
        {
            log_error(byLogNum, "read iFileFd error(%d)!", iFileBufLen);
            free(pDataBuf);
            free(pcFileName);
            free(pcBatchFile);
            return FAILE;
        }

        INT iWriteLen = write(iBatchFileFd, pDataBuf, iFileLen);
        if (iWriteLen == iFileLen) {
            //log_debug(byLogNum, "write batch file succeed.");
        } else {
            log_error(byLogNum, "write batch file error!");
        }
        
        if(remove(pcFileName) == 0){   
            //log_debug(byLogNum, "Removed %s.", pcFileName);
        } else {
            log_error(byLogNum, "remove file failed!");
        }
    
        close(iFileFd);
        
    }
    
    free(pDataBuf);
    free(pcFileName);
    free(pcBatchFile);
    
    close(iBatchFileFd);
    
    return SUCCESS;
}

static DWORD slave_dataBatch(void *pSlv, const void *pMsg)
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
    
    if(!(ntohl(pstReq->stData.stData.dwDataID) % 1000))
    {
        log_debug(byLogNum, "Receive batch pkg(%u), start:%u ; end:%u.", ntohl(pstReq->stData.stData.dwDataID),
                ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd));
    }
    if(ntohl(pstReq->stData.stData.dwDataID) == ntohl(pstReq->stData.dwDataEnd))
    {
        log_debug(byLogNum, "Receive last batch pkg(%u), start:%u ; end:%u.", ntohl(pstReq->stData.stData.dwDataID),
                ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd));
    }

    CHAR *pcFileName = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    sprintf(pcFileName, "batch_%u_to_%u", ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd));
    if(!access(pcFileName, 0))
    {
        //指定batch的文件已存在
        log_debug(byLogNum, "slave has already batched!");
        return SUCCESS;
    }

    free(pcFileName);

    //slave进入batch状态
    if(pclsSlv->stBatch.byBatchFlag == FALSE)
    {
        pclsSlv->stBatch.byBatchFlag = FALSE;
        pclsSlv->stBatch.wDataNums = 0;
        pclsSlv->stBatch.bySendtimes = 0;
        pclsSlv->stBatch.vecDataIDs.clear();

        //开batch定时器
        {

        }
    }

    DWORD dwRet = slave_writeBatchPkgs(pSlv, pstReq);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave_writeBatchPkgs failed!");
        return FAILE;
    }
    
    if(ntohl(pstReq->stData.stData.dwDataID) == ntohl(pstReq->stData.dwDataEnd))
    {    
        // slave统计未收到的batch包
        DWORD dwNeedPkg = slave_countLostPkgs(pSlv, ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd));
        if(dwNeedPkg == 0)
        {
            log_debug(byLogNum, "slave has received all batch pkg!");
            
            DWORD dwRet = slave_mergeBatchPkg(pSlv, ntohl(pstReq->stData.dwDataStart), ntohl(pstReq->stData.dwDataEnd));
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "slave_mergeBatchPkg() error!");
                return FAILE;
            }

            pclsSlv->stBatch.byBatchFlag = FALSE;
            pclsSlv->stBatch.wDataNums = 0;
            pclsSlv->stBatch.bySendtimes = 0;
            pclsSlv->stBatch.vecDataIDs.clear();
            //关batch定时器
            {

            }
        }
        else
        {
            log_debug(byLogNum, "slave still has %u batch pkg to recv!", dwNeedPkg);
        }
        // 向master发送batch反馈报文
        MSG_DATA_SLAVE_BATCH_RSP_S *pstRsp = (MSG_DATA_SLAVE_BATCH_RSP_S *)slave_alloc_rspMsg(ntohs(pstReq->stMsgHdr.wDstAddr), 
            ntohs(pstReq->stMsgHdr.wSrcAddr), START_SIG_2, ntohl(pstReq->stMsgHdr.dwSeq), CMD_DATA_BATCH);
        pstRsp->stSlvRecvResult.wNeedPkgNums = htons(dwNeedPkg);

        for(DWORD i = 0; i < dwNeedPkg; i++)
        {
            pstRsp->stSlvRecvResult.dwDataIDs[i] = htonl(pclsSlv->stBatch.vecDataIDs[i]);
        }
        
        DWORD dwRet = slave_sendMsg(pSlv, pclsSlv->wMstAddr, (void *)pstRsp, sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) + dwNeedPkg * sizeof(DWORD));
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "master_sendMsg error!");
            return FAILE;
        }
        
        free(pstRsp);
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

    if(wMsgLen < MSG_HDR_LEN)
    {
        log_error(byLogNum, "message length error(%u<%lu)!", wMsgLen, MSG_HDR_LEN);
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

        slave_msgHandleOne(pSlv, pstMsgHdr);//ntohs(pwSig[0])：START_SIG_1时为主机业务线程下发数据的消息，START_SIG_2时为备机主备线程回复的消息
        pwSig = (const WORD *)pbyMsg;
        if(wLeftLen <= 0)
        {
            break;
        }
    }

    return SUCCESS;
}

