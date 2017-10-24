#include <unistd.h> //for read STDIN_FILENO
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

DWORD main_stdinProc(void *pObj)
{
    log_debug("main_stdinProc().");
    DWORD dwRet = SUCCESS;

    return dwRet;
}

DWORD main_mailboxProc(void *pObj)
{
    log_debug("main_mailboxProc().");
    DWORD dwRet = SUCCESS;

    return dwRet;
}

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

    /* 业务线程中监听键盘事件与邮箱事件 */
    vos *pVos = new vos;
    DWORD dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error("vos_Init error!");
        return FAILE;
    }

    /* 向vos注册stdin事件 */
    dwRet = pVos->vos_RegTask("main_stdin", STDIN_FILENO, main_stdinProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 创建邮箱并注册到vos，实际上业务线程中的收应该是在发送数据后主动进行接收，而不应该交给异步触发 */
    dmm *pDmm = new dmm;
    mbufer *pMbufer = new mbufer;
    dwRet = pDmm->create_mailbox(&pMbufer, ADDR_10, "main_mb");//这里给定下发端的邮箱地址
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("main_mb", pMbufer->dwSocketFd, main_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 进入vos循环 */
    log_debug("main_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!

    /* free all */
    log_debug("Main Task Ending.");
    log_free();
    delete pVos;
    delete pDmm;
    delete pMbufer;
    return SUCCESS;
}


