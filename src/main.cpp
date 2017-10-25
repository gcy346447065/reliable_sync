#include <unistd.h> //for STDIN_FILENO read lseek close
#include <stdio.h> //for sscanf sprintf
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <fcntl.h> //for open
#include <sys/epoll.h> //for epoll
#include <errno.h> //for errno
#include <sys/socket.h> //for recv
#include <pthread.h> //for pthread
#include "macro.h"
#include "log.h"
#include "vos.h"
#include "mbufer.h"
#include "master.h"
#include "slave.h"

typedef struct
{
    BOOL bMstOrSlv;
    BYTE byMstAddr;
    BYTE bySlvAddr;
}SYNC_THREAD_S;

VOID *main_syncThread(VOID *pArg)
{
    log_debug("main_syncThread.");
    DWORD dwRet = SUCCESS;

    SYNC_THREAD_S *pstSyncThread = (SYNC_THREAD_S *)pArg;
    if(pstSyncThread->bMstOrSlv == TRUE)
    {
        //master开机初始化
        master *clsMst = new master(pstSyncThread->byMstAddr);
        clsMst->master_Init();

        //master循环
        clsMst->master_Loop();

        //master关机
        clsMst->master_Free();
    }
    else
    {
        //slave开机初始化
        slave *clsSlv = new slave(pstSyncThread->byMstAddr, pstSyncThread->bySlvAddr);
        clsSlv->slave_Init();

        //slave循环
        clsSlv->slave_Loop();

        //slave关机
        clsSlv->slave_Free();
    }

    return (VOID *)dwRet;
}

DWORD main_batchFile(DWORD dwFileNum, CHAR cTmpBuf)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_batchFile().");

    CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFilename, "%d%cfile", (INT)dwFileNum, cTmpBuf);

    INT iFileFd;
    if((iFileFd = open(pcFilename, O_RDONLY)) > 0)
    {
        //test_sendBatch(iFileFd);
    }
    else
    {
        log_info("open new config file error(%s)!", strerror(errno));
    }

    close(iFileFd);
    free(pcFilename);
    return dwRet;
}

DWORD main_waitedFile(DWORD dwFileNum)
{
    log_debug("test_waitedFile().");
    DWORD dwRet = SUCCESS;

    CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
    sprintf(pcFilename, "file%d", (INT)dwFileNum);

    INT iFileFd;
    if((iFileFd = open(pcFilename, O_RDONLY)) > 0)
    {
        //test_sendWaited(iFileFd);
    }
    else
    {
        log_info("open new config file error(%s)!", strerror(errno));
    }

    close(iFileFd);
    free(pcFilename);
    return dwRet;
}

DWORD reliableSync_send(void *pData, WORD wDataLen, DWORD dwTimeout)
{
    log_debug("reliableSync_send().");
    DWORD dwRet = SUCCESS;

    /*dwRet = g_clsTask->pMbufer->send_message(g_test_byMstAddr, stSendMsgInfo, wOffset);
    if (dwRet != SUCCESS)
    {
        log_error("send_message error!");
        g_clsTask->pMbufer->free_msg(pbySendBuf);
        return dwRet;
    }*/
    
    return dwRet;
}


DWORD task_stdinProc(void *pObj)
{
    log_debug("main_stdinProc().");
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
        main_batchFile((DWORD)iFileNum, acTmpBuf[0]);
    }
    else if(sscanf(pcStdinBuf, "?file%d", &iFileNum) == 1)
    {
        //log_debug("instantFile(%d)", iFileNum);
        //实时备份
        CHAR *pcFilename = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilename, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilename, "file%d", iFileNum);

        INT iFileFd;
        if((iFileFd = open(pcFilename, O_RDONLY)) < 0)
        {
            log_error("open new config file error(%s)!", strerror(errno));
            free(pcFilename);
            return FAILE;
        }
        else
        {
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

            dwRet = reliableSync_send(pbyFileBuf, wFileBufLen, 1000);//1000us
            if(dwRet != SUCCESS)
            {
                log_error("reliableSync_send error!");

                free(pbyFileBuf);
                return FAILE;
            }

            free(pbyFileBuf);
        }
    }
    else if(sscanf(pcStdinBuf, "/file%d", &iFileNum) == 1)
    {
        //log_debug("waitedFile(%d)", iFileNum);
        //定时定量备份
        main_waitedFile((DWORD)iFileNum);
    }

    free(pcStdinBuf);
    return dwRet;
}

class task
{
public:
    task(BOOL bMstOrSlv)
    {
        if(bMstOrSlv == TRUE)
        {
            byTaskAddr = ADDR_10;//master
        }
        else
        {
            byTaskAddr = ADDR_9;//slave
        }
    }

    DWORD task_Init()
    {
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

        /* 创建邮箱 */
        dwRet = pDmm->create_mailbox(&pMbufer, byTaskAddr, "task_mb");
        if(dwRet != SUCCESS)
        {
            log_error("create_mailbox error!");
            return FAILE;
        }

        /* 向vos注册stdin事件 */
        dwRet = pVos->vos_RegTask("task_stdin", STDIN_FILENO, task_stdinProc, NULL);
        if(dwRet != SUCCESS)
        {
            log_error("vos_RegTask error!");
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
        pVos->vos_EpollWait(); //while(1)!!!
    }

private:
    BYTE byTaskAddr;
    vos *pVos;
    dmm *pDmm;
    mbufer *pMbufer;
};

task *g_clsTask;

INT main(INT argc, CHAR *argv[])
{
    /* 开启log */
    log_init("");//现在用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_debug("Main Task Beginning.");

    /* 检查入参 */
    if(argc < 2)
    {
        log_error("main argc error!");
        log_free();
        return FAILE;
    }
    BOOL bMstOrSlv = TRUE;
    INT iMstAddr = 0, iSlvAddr = 0;
    if(strcmp(argv[1], "master") != SUCCESS && strcmp(argv[1], "slave") != SUCCESS)
    {
        log_error("main argv error!");
        log_free();
        return FAILE;
    }
    else if(strcmp(argv[1], "master") == SUCCESS)
    {
        bMstOrSlv = TRUE;
        if(sscanf(argv[2], "%d", &iMstAddr) != 1)
        {
            log_error("master addr error!");
            log_free();
            return FAILE;
        }
    }
    else if(strcmp(argv[1], "slave") == SUCCESS)
    {
        bMstOrSlv = FALSE;
        if(sscanf(argv[2], "%d", &iMstAddr) != 1)
        {
            log_error("master addr error!");
            log_free();
            return FAILE;
        }
        if(sscanf(argv[3], "%d", &iSlvAddr) != 1)
        {
            log_error("slave addr error!");
            log_free();
            return FAILE;
        }
    }
    //log_debug("bMstOrSlv(%d), byMstAddr(%d), bySlvAddr(%d).", bMstOrSlv, iMstAddr, iSlvAddr);

    /* 创建新线程作为主备线程，主线程作为业务线程 */
    pthread_t syncThreadId;
    SYNC_THREAD_S stSyncThread;
    stSyncThread.bMstOrSlv = bMstOrSlv;
    stSyncThread.byMstAddr = (BYTE)iMstAddr;
    stSyncThread.bySlvAddr = (BYTE)iSlvAddr;
    INT iRet = pthread_create(&syncThreadId, NULL, main_syncThread, (void *)&stSyncThread);
    if(iRet != SUCCESS)
    {
        log_error("pthread create error(%d)!", iRet);
        return FAILE;
    }

    /* 业务流程初始化 */
    g_clsTask = new task(bMstOrSlv);
    g_clsTask->task_Init();

    /* 业务流程epoll循环 */
    g_clsTask->task_Loop();

    /* 业务流程退出 */
    g_clsTask->task_Free();
    log_debug("Main Task Ending.");
    log_free();
    
    return SUCCESS;
}


