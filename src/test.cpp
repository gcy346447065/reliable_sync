#include <unistd.h> //for STDIN_FILENO lseek close
#include <stdio.h> //for sscanf
#include <stdlib.h> //for malloc free
#include <netinet/in.h> //for htons
#include <string.h> //for memset
#include <errno.h> //for errno
#include <fcntl.h> //for open
#include "test.h"
#include "log.h"
#include "protocol.h"


WORD g_wDataSeq = 0;

WORD g_wBatchID = 0;
WORD g_wInstantID = 0;
WORD g_wWaitedID = 0;

DWORD test_send(BYTE *pbyMsg, WORD wMsgLen)
{
    DWORD dwRet = SUCCESS;

    /* 申请mbufer发送消息体内存 */
    BYTE *pbySendBuf = NULL;
    dwRet = g_pTestMbufer->alloc_msg((VOID**)&pbySendBuf, (WORD)sizeof(CMD_S) + wMsgLen);
    if(dwRet != SUCCESS)
    {
        pbySendBuf = NULL;
        return dwRet;
    }
    dwRet = g_pTestMbufer->set_cmd_head_flag(pbySendBuf, CRM_HEADINFO_REQCMD);
    if(dwRet != SUCCESS)
    {
        g_pTestMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /* 将主备模块消息体与命令头消息封入mbufer发送消息体，并返回dwOffset留待后用 */
    WORD wOffset = 0;
    CMD_S stCmdHeader;
    stCmdHeader.wCmdIdx = 0;
    stCmdHeader.wCtrlCmd = 0x4000;
    stCmdHeader.wCmd = 1;
    stCmdHeader.wParaLen = wMsgLen;
    stCmdHeader.pbyPara = pbyMsg;
    dwRet = g_pTestMbufer->add_to_packet(pbySendBuf, &stCmdHeader, &wOffset);//可能是对于同一个pbySendBuf上面可能有多条stCmdHeader
    if (dwRet != SUCCESS)
    {
        g_pTestMbufer->free_msg(pbySendBuf);
        pbySendBuf = NULL;
        return dwRet;
    }

    /*INT iValue;
    socklen_t optlen;
    getsockopt(g_pTestMbufer->g_dwSocketFd, SOL_SOCKET, SO_SNDBUF, &iValue, &optlen);
    log_debug("SO_SNDBUF(%d)", iValue);*/

    /* 将mbufer发送消息体发送出去 */
    MSG_INFO_S stSendMsgInfo = {0, (DWORD)pbySendBuf, 0, 0};
    dwRet = g_pTestMbufer->send_message(g_test_byMstAddr, stSendMsgInfo, wOffset);
    if (dwRet != SUCCESS)
    {
        log_error("send_message error!");
        g_pTestMbufer->free_msg(pbySendBuf);
        return dwRet;
    }

    g_pTestMbufer->free_msg(pbySendBuf);
    return dwRet;
}

VOID *test_allocBatchMsg(WORD wPkgCount)
{
    log_debug("wPkgCount(%d)", wPkgCount);
    MSG_DATA_S *pstMsgData = (MSG_DATA_S *)malloc(sizeof(MSG_DATA_S) + MAX_PKG_LEN);
    if(pstMsgData)
    {
        pstMsgData->wDataSig = htons(START_FLAG_2); //数据起始标志signature
        pstMsgData->wDataSeq = htons(g_wDataSeq);   //数据序列号sequence
        pstMsgData->byDataType = DATA_TYPE_BATCH;
        //pstMsgData->wDataID = htons(0);
        pstMsgData->wBatchStart = htons(g_wBatchID);
        pstMsgData->wBatchEnd = htons(g_wBatchID + wPkgCount - 1);
        //pstMsgData->wDataLen = htons(wBufLen);
        pstMsgData->wDataChecksum = htons(0);
        //memcpy(pstMsgData->abyData, pbyBuf, wBufLen);
    }

    return (VOID *)pstMsgData;
}

VOID *test_allocInstantMsg(BYTE *pbyBuf, WORD wBufLen)
{
    log_debug("wBufLen(%d)", wBufLen);
    MSG_DATA_S *pstMsgData = (MSG_DATA_S *)malloc(sizeof(MSG_DATA_S) + wBufLen);
    if(pstMsgData)
    {
        pstMsgData->wDataSig = htons(START_FLAG_2); //数据起始标志signature
        pstMsgData->wDataSeq = htons(g_wDataSeq);   //数据序列号sequence
        pstMsgData->byDataType = DATA_TYPE_INSTANT;
        pstMsgData->wDataID = htons(g_wInstantID);
        pstMsgData->wBatchStart = htons(0);
        pstMsgData->wBatchEnd = htons(0);
        pstMsgData->wDataLen = htons(wBufLen);
        pstMsgData->wDataChecksum = htons(0);
        memcpy(pstMsgData->abyData, pbyBuf, wBufLen);
    }

    return (VOID *)pstMsgData;
}

VOID *test_allocWaitedMsg(BYTE *pbyBuf, WORD wBufLen)
{
    log_debug("wBufLen(%d)", wBufLen);
    MSG_DATA_S *pstMsgData = (MSG_DATA_S *)malloc(sizeof(MSG_DATA_S) + wBufLen);
    if(pstMsgData)
    {
        pstMsgData->wDataSig = htons(START_FLAG_2); //数据起始标志signature
        pstMsgData->wDataSeq = htons(g_wDataSeq);   //数据序列号sequence
        pstMsgData->byDataType = DATA_TYPE_WAITED;
        pstMsgData->wDataID = htons(g_wWaitedID);
        pstMsgData->wBatchStart = htons(0);
        pstMsgData->wBatchEnd = htons(0);
        pstMsgData->wDataLen = htons(wBufLen);
        pstMsgData->wDataChecksum = htons(0);
        memcpy(pstMsgData->abyData, pbyBuf, wBufLen);
    }

    return (VOID *)pstMsgData;
}

DWORD test_sendBatch(INT iFileFd)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_sendBatch()");

    BYTE *pbyFileBuf = (BYTE *)malloc(MAX_PKG_LEN);
    memset(pbyFileBuf, 0, MAX_PKG_LEN);

    INT iFileLen = lseek(iFileFd, 0, SEEK_END);//得到文件大小
    lseek(iFileFd, 0, SEEK_SET);//定位到文件头
    log_debug("iFileLen(%d).", iFileLen);

    g_wDataSeq++;
    WORD wPkgCount = (iFileLen + MAX_PKG_LEN - 1) / MAX_PKG_LEN;
    MSG_DATA_S *pstDataMsg = (MSG_DATA_S *)test_allocBatchMsg(wPkgCount);
    if(!pstDataMsg)
    {
        log_error("test_allocBatchMsg error!");

        free(pbyFileBuf);
        return FAILE;
    }

    for(INT i = 0; i < wPkgCount; i++)
    {
        INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_PKG_LEN);
        if(iFileBufLen < 0)
        {
            log_error("read iFileFd error(%d)!", iFileBufLen);

            free(pbyFileBuf);
            return FAILE;
        }
        WORD wFileBufLen = (WORD)iFileBufLen;
        
        pstDataMsg->wDataID = htons(g_wBatchID);
        pstDataMsg->wDataLen = htons(wFileBufLen);
        memcpy(pstDataMsg->abyData, pbyFileBuf, wFileBufLen);
        g_wBatchID++;
        
        dwRet = test_send((BYTE *)pstDataMsg, sizeof(MSG_DATA_S) + wFileBufLen);
        if(dwRet != SUCCESS)
        {
            log_error("test_send error!");

            free(pbyFileBuf);
            free(pstDataMsg);
            return FAILE;
        }

        //usleep(10 * 1);//延时100us以避免发送丢包
    }

    free(pbyFileBuf);
    free(pstDataMsg);
    return dwRet;
}

DWORD test_sendInstant(INT iFileFd)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_sendInstant()");

    BYTE *pbyFileBuf = (BYTE *)malloc(MAX_PKG_LEN);
    memset(pbyFileBuf, 0, MAX_PKG_LEN);
    INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_PKG_LEN);
    if(iFileBufLen < 0)
    {
        log_error("read iFileFd error(%d)!", iFileBufLen);

        free(pbyFileBuf);
        return FAILE;
    }
    WORD wFileBufLen = (WORD)iFileBufLen;

    g_wDataSeq++;
    g_wInstantID++;
    MSG_DATA_S *pstDataMsg = (MSG_DATA_S *)test_allocInstantMsg(pbyFileBuf, wFileBufLen);
    if(!pstDataMsg)
    {
        log_error("test_allocInstantMsg error!");

        free(pbyFileBuf);
        return FAILE;
    }

    dwRet = test_send((BYTE *)pstDataMsg, sizeof(MSG_DATA_S) + wFileBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("test_send error!");

        free(pbyFileBuf);
        free(pstDataMsg);
        return FAILE;
    }

    free(pstDataMsg);
    free(pbyFileBuf);
    return dwRet;
}

DWORD test_sendWaited(INT iFileFd)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_sendWaited()");

    BYTE *pbyFileBuf = (BYTE *)malloc(MAX_PKG_LEN);
    memset(pbyFileBuf, 0, MAX_PKG_LEN);
    INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_PKG_LEN);
    if(iFileBufLen < 0)
    {
        log_error("read iFileFd error(%d)!", iFileBufLen);

        free(pbyFileBuf);
        return FAILE;
    }
    WORD wFileBufLen = (WORD)iFileBufLen;

    g_wDataSeq++;
    g_wWaitedID++;
    MSG_DATA_S *pstDataMsg = (MSG_DATA_S *)test_allocWaitedMsg(pbyFileBuf, wFileBufLen);
    if(!pstDataMsg)
    {
        log_error("test_allocWaitedMsg error!");

        free(pbyFileBuf);
        return FAILE;
    }

    dwRet = test_send((BYTE *)pstDataMsg, sizeof(MSG_DATA_S) + wFileBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("test_send error!");

        free(pbyFileBuf);
        free(pstDataMsg);
        return FAILE;
    }

    free(pstDataMsg);
    free(pbyFileBuf);
    return dwRet;
}

DWORD test_batchFile(DWORD dwFileNum, CHAR cTmpBuf)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_batchFile().");

    CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFilename, "%d%cfile", (INT)dwFileNum, cTmpBuf);

    INT iFileFd;
    if((iFileFd = open(pcFilename, O_RDONLY)) > 0)
    {
        test_sendBatch(iFileFd);
    }
    else
    {
        log_info("open new config file error(%s)!", strerror(errno));
    }

    close(iFileFd);
    free(pcFilename);
    return dwRet;
}

DWORD test_instantFile(DWORD dwFileNum)
{
    log_debug("test_instantFile().");
    DWORD dwRet = SUCCESS;

    CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFilename, "file%d", (INT)dwFileNum);

    INT iFileFd;
    if((iFileFd = open(pcFilename, O_RDONLY)) > 0)
    {
        //test_sendInstant(iFileFd);
        log_debug("test_sendInstant()");

        INT iFileLen = lseek(iFileFd, 0, SEEK_END);//定位到文件尾以得到文件大小
        lseek(iFileFd, 0, SEEK_SET);//重新定位到文件头
        log_debug("iFileLen(%d).", iFileLen);
        if(iFileLen > MAX_PKG_LEN)//instant文件大小保证小于MAX_PKG_LEN，以保证能一次性发送
        {
            log_error("iFileLen(%d).", iFileLen);
            free(pcFilename);
            return FAILE;
        }

        BYTE *pbyFileBuf = (BYTE *)malloc(MAX_PKG_LEN);
        memset(pbyFileBuf, 0, MAX_PKG_LEN);
        INT iFileBufLen = read(iFileFd, pbyFileBuf, MAX_PKG_LEN);
        if(iFileBufLen < 0)
        {
            log_error("read iFileFd error(%d)!", iFileBufLen);
            free(pcFilename);
            free(pbyFileBuf);
            return FAILE;
        }
        WORD wFileBufLen = (WORD)iFileBufLen;

        
        

        dwRet = test_send((BYTE *)pstDataMsg, sizeof(MSG_DATA_S) + wFileBufLen);
        if(dwRet != SUCCESS)
        {
            log_error("test_send error!");

            free(pbyFileBuf);
            free(pstDataMsg);
            return FAILE;
        }

        free(pstDataMsg);
        free(pbyFileBuf);
    }
    else
    {
        log_info("open new config file error(%s)!", strerror(errno));
    }

    close(iFileFd);
    free(pcFilename);
    return dwRet;
}

DWORD test_waitedFile(DWORD dwFileNum)
{
    log_debug("test_waitedFile().");
    DWORD dwRet = SUCCESS;

    CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFilename, "file%d", (INT)dwFileNum);

    INT iFileFd;
    if((iFileFd = open(pcFilename, O_RDONLY)) > 0)
    {
        test_sendWaited(iFileFd);
    }
    else
    {
        log_info("open new config file error(%s)!", strerror(errno));
    }

    close(iFileFd);
    free(pcFilename);
    return dwRet;
}

DWORD test_stdinProc(void *pObj)
{
    log_debug("test_stdinProc().");
    DWORD dwRet = SUCCESS;

    //从控制台读取键入字符串
    CHAR *pcStdinBuf = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcStdinBuf, 0, MAX_STDIN_FILE_LEN);
    INT iRet = read(STDIN_FILENO, pcStdinBuf, MAX_STDIN_FILE_LEN);
    if(iRet <= 0)
    {
        log_error("read STDIN_FILENO error(%s)!", strerror(errno));
        return FAILE;
    }
    pcStdinBuf[iRet - 1] = '\0'; //-1 for '\n' turn to '\0'
    //log_debug("pcStdinBuf(%s)", pcStdinBuf);

    INT iFileNum = 0;
    CHAR acTmpBuf[2];
    if(sscanf(pcStdinBuf, "?%d%[KM]file", &iFileNum, acTmpBuf) == 2)//%[M]提取M是因为否则2Kfile也能进入第一个流程，这样对M敏感后可以避免此BUG
    {
        //log_debug("batchFile(%d,%s)", iFileNum, acTmpBuf);
        //批量备份，最大60M
        test_batchFile((DWORD)iFileNum, acTmpBuf[0]);
    }
    else if(sscanf(pcStdinBuf, "?file%d", &iFileNum) == 1)
    {
        //log_debug("instantFile(%d)", iFileNum);
        //实时备份
        test_instantFile((DWORD)iFileNum);
    }
    else if(sscanf(pcStdinBuf, "/file%d", &iFileNum) == 1)
    {
        //log_debug("waitedFile(%d)", iFileNum);
        //定时定量备份
        test_waitedFile((DWORD)iFileNum);
    }

    free(pcStdinBuf);
    return dwRet;
}

DWORD test_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_mailboxProc().");

    return dwRet;
}

DWORD test::test_Init()
{
    log_debug("test_Init begin.");
    DWORD dwRet = SUCCESS;

    pVos = new vos;
    dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error("vos_Init error!");
        return FAILE;
    }
    
    pDmm = new dmm;
    pMbufer = new mbufer;

    /* 创建邮箱并注册到vos */
    dwRet = pDmm->create_mailbox(&pMbufer, byTestAddr, "test_mb");
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("test_mb", pMbufer->dwSocketFd, test_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 向vos注册stdin事件 */
    dwRet = pVos->vos_RegTask("test_stdin", STDIN_FILENO, test_stdinProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    return dwRet;
}

VOID test::test_Free()
{
    log_debug("test_Free begin.");
    
    pVos->vos_free();
    delete pVos;
    pDmm->delete_mailbox(pMbufer);
    delete pDmm;
    delete pMbufer;

    return;
}

VOID test::test_Loop()
{
    /* 进入vos循环 */
    log_debug("test_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!
    
    return;
}

INT main(INT argc, CHAR *argv[])
{
	/* 开启log */
    log_init("TEST");
    log_info("TEST Beginning.");

	/* 检查入参 */
    if(argc != 2)
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }
    INT iMasterAddr = 0;
    if(sscanf(argv[1], "%d", &iMasterAddr) != 1)
    {
        log_error("master addr error!");
        log_free();
        return FAILE;
    }

	/* test初始化 */
    test *clsTest = new test(iMasterAddr);
    clsTest->test_Init();

    /* test循环 */
    clsTest->test_Loop();

    /* test关机 */
    clsTest->test_Free();
    return SUCCESS;
}

