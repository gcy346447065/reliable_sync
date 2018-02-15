#include <unistd.h> //for STDIN_FILENO read lseek close
#include <stdio.h> //for sscanf sprintf
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <fcntl.h> //for open
#include <errno.h> //for errno
#include <pthread.h> //for pthread
#include <netinet/in.h> //for htons
#include <iostream> //for cout
#include <sys/epoll.h> //for epoll
#include <sys/socket.h> //for recv
#include "macro.h"
#include "log.h"
#include "vos.h"
#include "mbufer.h"
#include "master.h"
#include "slave.h"
#include "protocol.h"

using namespace std;

DWORD task_stdinProc(void *pArg);
void *task_allocLogin(WORD wSrcAddr, WORD wDstAddr);
DWORD task_sendReliableSync(void *pArg, void *pData, WORD wDataLen, DWORD dwTimeout);

class task
{
public:
    task(BYTE byNum, BOOL bMstOrSlv, WORD wAddr)
    {
        if(bMstOrSlv == TRUE)
        {
            wTaskAddr = ADDR_MstTask; // master task
        }
        else
        {
            wTaskAddr = ADDR_SlvTask1; // slave task
        }

        wMstSlvAddr = wAddr;
        byLogNum = byNum;
    }

    DWORD task_Init()
    {
        DWORD dwRet = SUCCESS;

        /* 创建vos */
        pVos = new vos(byLogNum);
        dwRet = pVos->vos_Init(); // 实际为创建epoll
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "vos_Init error!");
            return FAILE;
        }
        
        /* 创建邮箱 */
        pDmm = new dmm(byLogNum);
        pMbufer = new mbufer(byLogNum);
        dwRet = pDmm->create_mailbox(&pMbufer, wTaskAddr, "task_mb");
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "create_mailbox error!");
            return FAILE;
        }

        /* 向vos注册stdin事件 */
        dwRet = pVos->vos_RegTask("task_stdin", STDIN_FILENO, task_stdinProc, this);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "vos_RegTask error!");
            return FAILE;
        }

        /* 向master或slave发送一次登录包，以方便master或slave记录wTaskAddr */
        sleep(1);
        MSG_LOGIN_REQ_S *pstLogin = (MSG_LOGIN_REQ_S *)task_allocLogin(wTaskAddr, wMstSlvAddr);
        if(!pstLogin)
        {
            log_error(byLogNum, "task_allocLogin error!");
            return FAILE;
        }
        //log_hex_8(byLogNum, pstLogin, 16);
        dwRet = task_sendReliableSync((void *)this, pstLogin, sizeof(MSG_LOGIN_REQ_S), MAX_TASK2MST_RECV_TIMEOUT);//单位us
        if(dwRet == FAILE)
        {
            log_error(byLogNum, "task_sendReliableSync error(%u)!", dwRet);
            return FAILE;
        }

        return dwRet;
    }
    
    VOID task_Free()
    {
        delete pVos;
        delete pDmm;
        delete pMbufer;
    }
    
    VOID task_Loop()
    {
        /* 进入vos循环 */
        pVos->vos_EpollWait(); // while(1)!!!
    }

private:
    vos *pVos;
    dmm *pDmm;
    
public:
    BYTE byLogNum;
    WORD wTaskAddr;
    WORD wMstSlvAddr; // 与task相连的master或者slave线程的地址
    mbufer *pMbufer;
};


typedef struct
{
    BOOL bMstOrSlv;
    WORD wMstAddr;
    WORD wSlvAddr;
}SYNC_THREAD_S;

static DWORD g_dwTskDataSeq = 1;
static DWORD g_dwTskBatchID = 0;
static DWORD g_dwTskInstantID = 0;
static DWORD g_dwTskWaitedID = 0;
WORD g_wByteBitCnt = 0;

DWORD task_countTypeBits(void) 
{
    //用于计算本机中一个BYTE类型所占用的位数
    BYTE bType = 1;
    while(bType != 0)
    {
        bType <<= 1;
        g_wByteBitCnt++;
    }
    
    return SUCCESS;
}

VOID *main_syncThread(VOID *pArg)
{
    SYNC_THREAD_S *pstSyncThread = (SYNC_THREAD_S *)pArg;
    if(pstSyncThread->bMstOrSlv == TRUE)
    {
        //master开机初始化
        master *pclsMst = new master(LOG2, pstSyncThread->wMstAddr);
        pclsMst->master_Init();

        //master循环
        pclsMst->master_Loop();

        //master关机
        pclsMst->master_Free();
    }
    else
    {
        //slave开机初始化
        slave *pclsSlv = new slave(LOG2, pstSyncThread->wMstAddr, pstSyncThread->wSlvAddr);
        pclsSlv->slave_Init();
        
        //slave循环
        pclsSlv->slave_Loop();

        //slave关机
        pclsSlv->slave_Free();
    }

    return (VOID *)pArg;
}

DWORD task_sendReliableSync(void *pArg, void *pData, WORD wDataLen, DWORD dwTimeout)
{
    DWORD dwRet = SUCCESS;
    task *pclsTask = (task *)pArg;
    BYTE byLogNum = pclsTask->byLogNum;
    BYTE wMstSlvAddr = pclsTask->wMstSlvAddr;
    mbufer *pMbufer = pclsTask->pMbufer;
    //log_debug(byLogNum, "task_sendReliableSync().");
    
    /* 向主机主备线程接收端口发送数据 */
    dwRet = pMbufer->send_message(wMstSlvAddr, pData, wDataLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "send_message error!");
        return dwRet;
    }

    /* 等待主机主备线程接收端口的回复，超时作失败对待 */
    void *pRecvBuf = malloc(MAX_RECV_LEN);
    WORD wRecvBufLen = MAX_RECV_LEN;

    dwRet = pMbufer->receive_message(pRecvBuf, &wRecvBufLen, dwTimeout);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "receive_message error!");
        free(pRecvBuf);
        return dwRet;
    }

    if(wRecvBufLen == 0)
    {
        //说明recv超时返回0
        log_warning(byLogNum, "receive_message return with 0.");
        free(pRecvBuf);
        return FAILE;
    }

    MSG_HDR_S *pstMsgHdr = (MSG_HDR_S *)pRecvBuf;
    if(ntohs(pstMsgHdr->wCmd) == CMD_LOGIN)
    {
        log_hex_8(byLogNum, pRecvBuf, (WORD)sizeof(MSG_LOGIN_RSP_S));
        MSG_LOGIN_RSP_S *pstRsp = (MSG_LOGIN_RSP_S *)pRecvBuf;
        if(pstRsp->byLoginResult == LOGIN_RESULT_SUCCEED)
        {
            log_debug(byLogNum, "MSG_LOGIN_RSP_S succeed.");
        }
        else
        {
            log_debug(byLogNum, "MSG_LOGIN_RSP_S error.");
        }
    }
    else if(ntohs(pstMsgHdr->wCmd) == CMD_DATA_BATCH)
    {
        //log_hex_8(byLogNum, pRecvBuf, (WORD)sizeof(MSG_DATA_BATCH_RSP_S));
        MSG_DATA_BATCH_RSP_S *pstRsp = (MSG_DATA_BATCH_RSP_S *)pRecvBuf;
        if(pstRsp->stDataResult.byResult == DATA_RESULT_SUCCEED)
        {
            //log_debug(byLogNum, "MSG_DATA_BATCH_RSP_S succeed.");
        }
        else
        {
            log_debug(byLogNum, "MSG_DATA_BATCH_RSP_S error.");
        }
    }
    else if(ntohs(pstMsgHdr->wCmd) == CMD_DATA_INSTANT)
    {
        log_hex_8(byLogNum, pRecvBuf, (WORD)sizeof(MSG_DATA_INSTANT_RSP_S));
        MSG_DATA_INSTANT_RSP_S *pstRsp = (MSG_DATA_INSTANT_RSP_S *)pRecvBuf;
        if(pstRsp->stDataResult.byResult == DATA_RESULT_SUCCEED)
        {
            log_debug(byLogNum, "MSG_DATA_INSTANT_RSP_S succeed.");
        }
        else
        {
            log_debug(byLogNum, "MSG_DATA_INSTANT_RSP_S error.");
        }
    }
    else if(ntohs(pstMsgHdr->wCmd) == CMD_DATA_WAITED)
    {
        log_hex_8(byLogNum, pRecvBuf, (WORD)sizeof(MSG_DATA_WAITED_RSP_S));
        MSG_DATA_WAITED_RSP_S *pstRsp = (MSG_DATA_WAITED_RSP_S *)pRecvBuf;
        if(pstRsp->stDataResult.byResult == DATA_RESULT_SUCCEED)
        {
            log_debug(byLogNum, "MSG_DATA_WAITED_RSP_S succeed.");
        }
        else
        {
            log_debug(byLogNum, "MSG_DATA_WAITED_RSP_S error.");
        }
    }
    else if(ntohs(pstMsgHdr->wCmd) == CMD_GET_DATA_COUNT)
    {
        DWORD dwBatchCount = 0, dwInstantCount = 0, dwWaitedCount = 0;
        MSG_GET_DATA_COUNT_RSP_S *pstRsp = (MSG_GET_DATA_COUNT_RSP_S *)pRecvBuf;
        dwBatchCount = ntohl(pstRsp->dwBatchCount);
        dwInstantCount = ntohl(pstRsp->dwInstantCount);
        dwWaitedCount = ntohl(pstRsp->dwWaitedCount);

        cout << "get data count: Batch-" << dwBatchCount << ", Instant-" << dwInstantCount << ", Waited-" << dwWaitedCount << endl;
        log_debug(byLogNum, "get data count: Batch(%u), Instant(%u), Waited(%u).", dwBatchCount, dwInstantCount, dwWaitedCount);
    }

    free(pRecvBuf);
    return dwRet;
}

void *task_allocLogin(WORD wSrcAddr, WORD wDstAddr)
{
    //log_debug(LOG1, "wPkgCount(%d).", wPkgCount);
    MSG_LOGIN_REQ_S *pstLogin = (MSG_LOGIN_REQ_S *)malloc(sizeof(MSG_LOGIN_REQ_S));
    if(pstLogin)
    {
        pstLogin->stMsgHdr.wSig = htons(START_SIG_1);
        pstLogin->stMsgHdr.wVer = htons(VERSION_INT);
        pstLogin->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstLogin->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstLogin->stMsgHdr.dwSeq = htonl(g_dwTskDataSeq++);
        pstLogin->stMsgHdr.wCmd = htons(CMD_LOGIN);
        pstLogin->stMsgHdr.wLen = htons(0);
    }

    return (void *)pstLogin;
}

//此函数申请的内存会在同一次批量备份时复用多次
void *task_allocDataBatch(WORD wSrcAddr, WORD wDstAddr, DWORD dwPkgCount)
{
    if(g_dwTskBatchID + dwPkgCount - 1 < g_dwTskBatchID)
    {
        //ID翻转
        g_dwTskBatchID = 0;
    }

    MSG_DATA_BATCH_REQ_S *pstBatch = (MSG_DATA_BATCH_REQ_S *)malloc(sizeof(MSG_DATA_BATCH_REQ_S) + MAX_TASK2MST_PKG_LEN);
    if(pstBatch)
    {
        pstBatch->stMsgHdr.wSig = htons(START_SIG_1);
        pstBatch->stMsgHdr.wVer = htons(VERSION_INT);
        pstBatch->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstBatch->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstBatch->stMsgHdr.dwSeq = htonl(g_dwTskDataSeq++);
        pstBatch->stMsgHdr.wCmd = htons(CMD_DATA_BATCH);
        pstBatch->stMsgHdr.wLen = htons(0);//后面在循环中写入

        pstBatch->stData.dwDataStart = htonl(g_dwTskBatchID);
        pstBatch->stData.dwDataEnd = htonl(g_dwTskBatchID + dwPkgCount - 1);

        pstBatch->stData.stData.dwDataID = htonl(0);//后面在循环中写入
        pstBatch->stData.stData.wDataLen = htons(0);//后面在循环中写入
        pstBatch->stData.stData.wDataChecksum = htons(0);//在主机下发时不填
        //pstBatch->stData.stData.abyData在循环中写入
    }

    return (void *)pstBatch;
}

void *task_allocDataInstant(WORD wSrcAddr, WORD wDstAddr, void *pBuf, WORD wBufLen)
{
    WORD wMsgLen = sizeof(MSG_DATA_INSTANT_REQ_S) + wBufLen;
    //log_debug(LOG1, "wBufLen(%d), wMsgLen(%d).", wBufLen, wMsgLen);
    MSG_DATA_INSTANT_REQ_S *pstInstant = (MSG_DATA_INSTANT_REQ_S *)malloc(wMsgLen);
    if(pstInstant)
    {
        pstInstant->stMsgHdr.wSig = htons(START_SIG_1);
        pstInstant->stMsgHdr.wVer = htons(VERSION_INT);
        pstInstant->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstInstant->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstInstant->stMsgHdr.dwSeq = htonl(g_dwTskDataSeq++);
        pstInstant->stMsgHdr.wCmd = htons(CMD_DATA_INSTANT);
        pstInstant->stMsgHdr.wLen = htons(wMsgLen - MSG_HDR_LEN);

        pstInstant->stData.dwDataID = htonl(g_dwTskInstantID++);
        pstInstant->stData.wDataLen = htons(wBufLen);
        pstInstant->stData.wDataChecksum = htons(0);//在主机下发时不填
        memcpy(pstInstant->stData.abyData, pBuf, wBufLen);
    }

    return (void *)pstInstant;
}

void *task_allocDataWaited(WORD wSrcAddr, WORD wDstAddr, void *pBuf, WORD wBufLen)
{
    WORD wMsgLen = sizeof(MSG_DATA_INSTANT_REQ_S) + wBufLen;
    //log_debug(LOG1, "wBufLen(%d), wMsgLen(%d).", wBufLen, wMsgLen);
    /*
     * !!!需要注意的是MSG_DATA_WAITED_REQ_S用于主备机之间打包发送数据，
     * 这里主机业务线程向主备线程下发waited数据时仍用MSG_DATA_INSTANT_REQ_S单次发送，
     * 但是wCmd不同，dwDataID不共用。
     */
    MSG_DATA_INSTANT_REQ_S *pstWaited = (MSG_DATA_INSTANT_REQ_S *)malloc(wMsgLen);
    if(pstWaited)
    {
        pstWaited->stMsgHdr.wSig = htons(START_SIG_1);
        pstWaited->stMsgHdr.wVer = htons(VERSION_INT);
        pstWaited->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstWaited->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstWaited->stMsgHdr.dwSeq = htonl(g_dwTskDataSeq++);
        pstWaited->stMsgHdr.wCmd = htons(CMD_DATA_WAITED);
        pstWaited->stMsgHdr.wLen = htons(wMsgLen - MSG_HDR_LEN);

        pstWaited->stData.dwDataID = htonl(g_dwTskWaitedID++);
        pstWaited->stData.wDataLen = htons(wBufLen);
        pstWaited->stData.wDataChecksum = htons(0);//在主机下发时不填
        memcpy(pstWaited->stData.abyData, pBuf, wBufLen);
    }

    return (void *)pstWaited;
}

void *task_allocGetDataCount(WORD wSrcAddr, WORD wDstAddr)
{
    MSG_GET_DATA_COUNT_REQ_S *pstReq = (MSG_GET_DATA_COUNT_REQ_S *)malloc(sizeof(MSG_GET_DATA_COUNT_REQ_S));
    if(pstReq)
    {
        pstReq->stMsgHdr.wSig = htons(START_SIG_1);
        pstReq->stMsgHdr.wVer = htons(VERSION_INT);
        pstReq->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstReq->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstReq->stMsgHdr.dwSeq = htonl(g_dwTskDataSeq++);
        pstReq->stMsgHdr.wCmd = htons(CMD_GET_DATA_COUNT);
        pstReq->stMsgHdr.wLen = htons(0);
    }

    return (void *)pstReq;
}

DWORD task_stdinProc(void *pArg)
{
    DWORD dwRet = SUCCESS;
    task *pclsTask = (task *)pArg;
    BYTE byLogNum = pclsTask->byLogNum;
    WORD wTaskAddr = pclsTask->wTaskAddr;
    WORD wMstSlvAddr = pclsTask->wMstSlvAddr;
    log_debug(byLogNum, "task_stdinProc().");    

    //从控制台读取键入字符串
    CHAR *pcStdinBuf = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcStdinBuf, 0, MAX_STDIN_FILE_LEN);
    INT iRet = read(STDIN_FILENO, pcStdinBuf, MAX_STDIN_FILE_LEN);
    if(iRet < 0)
    {
        log_error(byLogNum, "read STDIN_FILENO error(%s)!", strerror(errno));
        free(pcStdinBuf);
        return FAILE;
    }
    pcStdinBuf[iRet - 1] = '\0'; //-1 for '\n' turn to '\0'
    //log_debug(byLogNum, "pcStdinBuf(%s)", pcStdinBuf);

    INT iFileNum = 0;
    CHAR acTmpBuf[2];
    if(sscanf(pcStdinBuf, "?%d%[KM]file", &iFileNum, acTmpBuf) == 2)//%[M]提取M是因为否则2Kfile也能进入第一个流程，这样对M敏感后可以避免此BUG
    {
        //log_debug(byLogNum, "batchFile(%d,%s)", iFileNum, acTmpBuf);
        //批量备份，最大60M
        cout << "Batch backuping, please wait..." << endl;
        
        CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilename, "%d%cfile", iFileNum, acTmpBuf[0]);

        INT iFileFd;
        if((iFileFd = open(pcFilename, O_RDONLY)) < 0)
        {
            cout << "open new config file error" << endl;
            log_error(byLogNum, "open new config file error(%s)!", strerror(errno));
            free(pcStdinBuf);
            free(pcFilename);
            return FAILE;
        }
        else
        {
            INT iFileLen = lseek(iFileFd, 0, SEEK_END);//定位到文件尾以得到文件大小
            lseek(iFileFd, 0, SEEK_SET);//重新定位到文件头
            log_debug(byLogNum, "batch iFileLen(%d).", iFileLen);

            DWORD dwPkgCount = (iFileLen + MAX_TASK2MST_PKG_LEN - 1) / MAX_TASK2MST_PKG_LEN + 1;
            MSG_DATA_BATCH_REQ_S *pstBatch = (MSG_DATA_BATCH_REQ_S *)task_allocDataBatch(wTaskAddr, wMstSlvAddr, dwPkgCount);
            if(!pstBatch)
            {
                log_error(byLogNum, "task_allocDataBatch error!");
                free(pcStdinBuf);
                free(pcFilename);
                return FAILE;
            }
            
            log_debug(byLogNum, "dwPkgCount = %u.", dwPkgCount);
            
            for(DWORD i = 0; i < dwPkgCount; i++) //i=0时为IDStart，传送batch文件名
            {
                void *pDataBuf = (void *)pstBatch->stData.stData.abyData;
                memset(pDataBuf, 0, MAX_TASK2MST_PKG_LEN);

                WORD wDataLen = 0;
                if(i == 0)
                {
                    wDataLen = (WORD)strlen(pcFilename);
                    memcpy((void *)pDataBuf, (void *)pcFilename, wDataLen);
                    
                    log_debug(byLogNum, "batch file(%s).", pcFilename);
                }
                else
                {
                    INT iFileBufLen = read(iFileFd, pDataBuf, MAX_TASK2MST_PKG_LEN);
                    if(iFileBufLen < 0)
                    {
                        log_error(byLogNum, "read iFileFd error(%d)!", iFileBufLen);
                        free(pcStdinBuf);
                        free(pcFilename);
                        free(pstBatch);
                        return FAILE;
                    }
                    wDataLen = (WORD)iFileBufLen;
                }
                
                WORD wBatchLen = sizeof(MSG_DATA_BATCH_REQ_S) + wDataLen;
                pstBatch->stMsgHdr.wLen = htons(wBatchLen - MSG_HDR_LEN);//在循环中每次重新写值
                pstBatch->stData.stData.dwDataID = htonl(g_dwTskBatchID++);
                pstBatch->stData.stData.wDataLen = htons(wDataLen);

                //log_hex_8(byLogNum, pstBatch, 32);
                iRet = task_sendReliableSync(pArg, pstBatch, wBatchLen, MAX_TASK2MST_RECV_TIMEOUT);//单位us
                if(iRet == FAILE)
                {
                    log_error(byLogNum, "task_sendReliableSync error(%d)!", iRet);
                    free(pcStdinBuf);
                    free(pcFilename);
                    free(pstBatch);
                    return FAILE;
                }
                
            }

            cout << "batch: " << pcFilename << endl;
            free(pstBatch);
            free(pcFilename);
        }
        close(iFileFd);
    }
    else if(sscanf(pcStdinBuf, "?file%d", &iFileNum) == 1)
    {
        //log_debug(byLogNum, "instantFile(%d)", iFileNum);
        //实时备份
        CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilename, "file%d", iFileNum);

        INT iFileFd;
        if((iFileFd = open(pcFilename, O_RDONLY)) < 0)
        {
            cout << "open new config file error" << endl;
            log_error(byLogNum, "open new config file error(%s)!", strerror(errno));
            free(pcStdinBuf);
            free(pcFilename);
            return FAILE;
        }
        else
        {
            INT iFileLen = lseek(iFileFd, 0, SEEK_END);//定位到文件尾以得到文件大小
            lseek(iFileFd, 0, SEEK_SET);//重新定位到文件头
            log_debug(byLogNum, "instant iFileLen(%d).", iFileLen);
            
            if(iFileLen > MAX_FILE_LEN) //instant文件大小保证小于MAX_FILE_LEN，否则不发送
            {
                cout << "file length should limited to " << MAX_FILE_LEN << endl;
                log_error(byLogNum, "iFileLen(%d) is too large!", iFileLen);
                free(pcStdinBuf);
                free(pcFilename);
                return FAILE;
            }

            BYTE *pbyFileBuf = (BYTE *)malloc(MAX_FILE_LEN);
            memset(pbyFileBuf, 0, MAX_FILE_LEN);
            INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_FILE_LEN);
            if(iFileBufLen < 0)
            {
                log_error(byLogNum, "read iFileFd error(%d)!", iFileBufLen);
                free(pcStdinBuf);
                free(pcFilename);
                free(pbyFileBuf);
                return FAILE;
            }
            WORD wFileBufLen = (WORD)iFileBufLen;

            MSG_DATA_INSTANT_REQ_S *pstInstant = (MSG_DATA_INSTANT_REQ_S *)task_allocDataInstant(wTaskAddr, wMstSlvAddr, pbyFileBuf, wFileBufLen);
            if(!pstInstant)
            {
                log_error(byLogNum, "task_allocDataInstant error!");
                free(pcStdinBuf);
                free(pcFilename);
                free(pbyFileBuf);
                return FAILE;
            }
            free(pbyFileBuf);

            dwRet = task_sendReliableSync(pArg, pstInstant, sizeof(MSG_DATA_INSTANT_REQ_S) + wFileBufLen, MAX_TASK2MST_RECV_TIMEOUT);//单位us
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "reliableSync_send error!");
                free(pcStdinBuf);
                free(pcFilename);
                return FAILE;
            }

            cout << "instant: " << pcFilename << endl;
            free(pcFilename);
        }
        close(iFileFd);
    }
    else if(sscanf(pcStdinBuf, "/file%d", &iFileNum) == 1)
    {
        //log_debug(byLogNum, "waitedFile(%d)", iFileNum);
        //定时定量备份
        CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilename, "file%d", iFileNum);

        INT iFileFd;
        if((iFileFd = open(pcFilename, O_RDONLY)) < 0)
        {
            cout << "open new config file error" << endl;
            log_error(byLogNum, "open new config file error(%s)!", strerror(errno));
            free(pcStdinBuf);
            free(pcFilename);
            return FAILE;
        }
        else
        {
            INT iFileLen = lseek(iFileFd, 0, SEEK_END);//定位到文件尾以得到文件大小
            lseek(iFileFd, 0, SEEK_SET);//重新定位到文件头
            log_debug(byLogNum, "waited iFileLen(%d).", iFileLen);
            
            if(iFileLen > MAX_FILE_LEN)//waited文件大小与instant一致
            {
                cout << "file length should limited to " << MAX_FILE_LEN << endl;
                log_error(byLogNum, "iFileLen(%d).", iFileLen);
                free(pcStdinBuf);
                free(pcFilename);
                return FAILE;
            }

            BYTE *pbyFileBuf = (BYTE *)malloc(MAX_FILE_LEN);
            memset(pbyFileBuf, 0, MAX_FILE_LEN);
            INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_FILE_LEN);
            if(iFileBufLen < 0)
            {
                log_error(byLogNum, "read iFileFd error(%d)!", iFileBufLen);
                free(pcStdinBuf);
                free(pcFilename);
                free(pbyFileBuf);
                return FAILE;
            }
            WORD wFileBufLen = (WORD)iFileBufLen;

            MSG_DATA_WAITED_REQ_S *pstWaited = (MSG_DATA_WAITED_REQ_S *)task_allocDataWaited(wTaskAddr, wMstSlvAddr, pbyFileBuf, wFileBufLen);
            if(!pstWaited)
            {
                log_error(byLogNum, "task_allocDataWaited error!");
                free(pcStdinBuf);
                free(pcFilename);
                free(pbyFileBuf);
                return FAILE;
            }
            free(pbyFileBuf);

            dwRet = task_sendReliableSync(pArg, pstWaited, sizeof(MSG_DATA_WAITED_REQ_S) + wFileBufLen, MAX_TASK2MST_RECV_TIMEOUT);//单位us
            if(dwRet != SUCCESS)
            {
                log_error(byLogNum, "reliableSync_send error!");
                free(pcStdinBuf);
                free(pcFilename);
                return FAILE;
            }

            cout << "waited: " << pcFilename << endl;
            free(pcFilename);
        }
        close(iFileFd);
    }
    else if(strcmp(pcStdinBuf, "get") == 0)
    {
        MSG_GET_DATA_COUNT_REQ_S *pstReq = (MSG_GET_DATA_COUNT_REQ_S *)task_allocGetDataCount(wTaskAddr, wMstSlvAddr);
        if(!pstReq)
        {
            log_error(byLogNum, "task_allocDataWaited error!");
            free(pcStdinBuf);
            return FAILE;
        }

        dwRet = task_sendReliableSync(pArg, pstReq, sizeof(MSG_GET_DATA_COUNT_REQ_S), MAX_TASK2MST_RECV_TIMEOUT);//单位us
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "reliableSync_send error!");
            free(pcStdinBuf);
            return FAILE;
        }
    }

    free(pcStdinBuf);
    return dwRet;
}

INT main(INT argc, CHAR *argv[])
{
    /* 开启log */
    BYTE byLogNum = LOG1;
    log_init(byLogNum, ""); // 现在用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_debug(byLogNum, "Main Task Beginning.");

    /* 检查入参 */
    if(argc < 2)
    {
        cout << "ERROR: main argc error!" << endl;
        log_error(byLogNum, "main argc error!");
        log_free();
        return FAILE;
    }

    BOOL bMstOrSlv = TRUE;
    INT iMstAddr = ADDR_1_114, iSlvAddr = ADDR_6_119;
    if(strcmp(argv[1], "master") == SUCCESS)
    {
        bMstOrSlv = TRUE;

        if (argc == 2) {
            cout << "DEFAULT: master 1" << endl;
        }else if (argc == 3) {
            if (sscanf(argv[2], "%d", &iMstAddr) != 1) {
                cout << "ERROR: master addr error!" << endl;
                log_error(byLogNum, "master addr error!");
                log_free();
                return FAILE;
            }
            if (iMstAddr < 1 || iMstAddr > 10) {
                cout << "ERROR: master/slave 1-10 1-10" << endl;
                log_free();
                return FAILE;
            }
            cout << "USER: master " << iMstAddr << endl;
        } else if (argc > 3) {
            cout << "ERROR: main argc error!" << endl;
            log_error(byLogNum, "main argc error!");
            log_free();
            return FAILE;
        }
    }
    else if(strcmp(argv[1], "slave") == SUCCESS)
    {
        bMstOrSlv = FALSE;

        if (argc == 2) {
            cout << "DEFAULT: slave 1 6" << endl;
        }else if (argc == 3) {
            if(sscanf(argv[2], "%d", &iMstAddr) != 1)
            {
                cout << "ERROR: master addr error!" << endl;
                log_error(byLogNum, "master addr error!");
                log_free();
                return FAILE;
            }
            if (iMstAddr < 1 || iMstAddr > 10) {
                cout << "ERROR: master/slave 1-10 1-10" << endl;
                log_free();
                return FAILE;
            }
            cout << "USER: slave " << iMstAddr << " 6" << endl;
        }else if (argc == 4) {
            if(sscanf(argv[2], "%d", &iMstAddr) != 1)
            {
                cout << "ERROR: master addr error!" << endl;
                log_error(byLogNum, "master addr error!");
                log_free();
                return FAILE;
            }
            if (iMstAddr < 1 || iMstAddr > 10) {
                cout << "ERROR: master/slave 1-10 1-10" << endl;
                log_free();
                return FAILE;
            }
            if(sscanf(argv[3], "%d", &iSlvAddr) != 1)
            {
                cout << "ERROR: slave addr error!" << endl;
                log_error(byLogNum, "slave addr error!");
                log_free();
                return FAILE;
            }
            if (iSlvAddr < 1 || iSlvAddr > 10) {
                cout << "ERROR: master/slave 1-10 1-10" << endl;
                log_free();
                return FAILE;
            }
            cout << "USER: slave " << iMstAddr << " " << iSlvAddr << endl;
        } else if (argc > 4) {
            cout << "ERROR: main argc error!" << endl;
            log_error(byLogNum, "main argc error!");
            log_free();
            return FAILE;
        }
    }
    else
    {
        cout << "ERROR: main argv error!" << endl;
        log_error(byLogNum, "main argv error!");
        log_free();
        return FAILE;
    }
    
    task_countTypeBits();
    log_debug(byLogNum, "short(%lu), int(%lu), long(%lu), long long(%lu).", sizeof(short), sizeof(int), sizeof(long), sizeof(long long));
    log_debug(byLogNum, "WORD(%lu), DWORD(%lu), QWORD(%lu).", sizeof(WORD), sizeof(DWORD), sizeof(QWORD));
    log_debug(byLogNum, "A BYTE has %u bits.", g_wByteBitCnt);

    /* 创建新线程作为主备线程，主线程作为业务线程 */
    pthread_t syncThreadId;
    SYNC_THREAD_S stSyncThread;
    stSyncThread.bMstOrSlv = bMstOrSlv;
    stSyncThread.wMstAddr = (WORD)iMstAddr;
    stSyncThread.wSlvAddr = (WORD)iSlvAddr;
    INT iRet = pthread_create(&syncThreadId, NULL, main_syncThread, (void *)&stSyncThread);
    if(iRet != SUCCESS)
    {
        log_error(byLogNum, "pthread create error(%d)!", iRet);
        return FAILE;
    }

    /* 业务流程初始化 */
    task *pclsTask = new task(byLogNum, bMstOrSlv, bMstOrSlv ? (WORD)iMstAddr : (WORD)iSlvAddr);
    pclsTask->task_Init();

    /* 业务流程epoll循环 */
    pclsTask->task_Loop();

    /* 业务流程退出 */
    pclsTask->task_Free();
    log_debug(byLogNum, "Main Task Ending.");
    log_free();
    
    return SUCCESS;
}

