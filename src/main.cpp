#include <unistd.h> //for read STDIN_FILENO
#include <stdio.h> //for sscanf sprintf
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <fcntl.h> //for open
#include <sys/epoll.h> //for epoll
#include <errno.h> //for errno
#include "macro.h"
#include "log.h"
#include "event.h"
#include "timer.h"
#include "vos.h"
#include "mbufer.h"

DWORD master_MonitorFromSlave();
DWORD slave_RegisterToMaster();

/*
 * 主备服务的主线程，主要负责监听创建具体服务的请求
 */
INT main(INT argc, CHAR *argv[])
{
    //现在用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_init("");
    log_info("Task Beginning.");

    if(argc != 2)
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }

    DWORD dwRet;
    if(strcmp(argv[1], "master") == SUCCESS)
    {
        log_init("MASTER MAIN");
        log_info("MASTER MAIN Task.");

        //一直监听备机发来的创建主服务子线程的请求，成功后创建主服务子线程与备服务进行通讯
        master_MonitorFromSlave();
    }
    else if(strcmp(argv[1], "slave") == SUCCESS)
    {
        log_init("SLAVE MAIN");
        log_info("SLAVE MAIN Task.");

        //循环向主机发送注册包，成功后与主服务子线程进行通讯
        slave_RegisterToMaster();
    }
    else
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }

    /* free all */
    log_info("Task Ending.");
    log_free();
    return FAILE;
}

DWORD master_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;

    log_debug("master_mailboxProc()");

    return dwRet;
}

DWORD master_MonitorFromSlave()
{
    DWORD dwRet = SUCCESS;

    vos *pMasterVos = new vos;//vos相当于是epoll
    dwRet = pMasterVos->VOS_Init();
    if(dwRet != SUCCESS)
    {
        log_error("VOS_Init error!");
        return FAILE;
    }

    mbufer *pMasterMbufer = new mbufer;
    MSG_ADDR MailboxAddr    = {0};
    MailboxAddr.dwNeID      = 1;
    MailboxAddr.wCardID     = 2;
    MailboxAddr.wSoftwareID = 3;
    MailboxAddr.byEntryID   = ADDR_1;//实际只使用该位对应ip加端口号

    dmm *pMasterDmm = new dmm;//实际上在create_mailbox中确定mbufer中的g_dwMbuferFd，也就是对应vos中的dwTaskEventFd
    dwRet = pMasterDmm->create_mailbox(&pMasterMbufer, MailboxAddr);
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }

    dwRet = pMasterVos->VOS_RegTaskEventFd(VOS_TASK_MASTER_MAILBOX, pMasterMbufer->g_dwMbuferFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }

    dwRet = pMasterVos->VOS_RegTaskFunc(VOS_TASK_MASTER_MAILBOX, master_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    pMasterVos->VOS_EpollWait(); //while(1)!!!

    return dwRet;
}

DWORD slave_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;

    log_debug("slave_mailboxProc()");

    return dwRet;
}

DWORD slave_registerTimerProc(void *pObj)
{
    DWORD dwRet = SUCCESS;

    log_debug("slave_registerTimerProc()");

    return dwRet;
}

DWORD slave_RegisterToMaster()
{
    DWORD dwRet = SUCCESS;

    vos *pSlaveVos = new vos;
    dwRet = pSlaveVos->VOS_Init();
    if(dwRet != SUCCESS)
    {
        log_error("VOS_Init error!");
        return FAILE;
    }

    /* 备机邮箱添加到vos */
    mbufer *pSlaveMbufer = new mbufer;
    MSG_ADDR MailboxAddr    = {0};
    MailboxAddr.dwNeID      = 1;
    MailboxAddr.wCardID     = 2;
    MailboxAddr.wSoftwareID = 3;
    MailboxAddr.byEntryID   = ADDR_2;//实际只使用该位对应ip加端口号

    dmm *pSlaveDmm = new dmm;//实际上在create_mailbox中确定mbufer中的g_dwMbuferFd，也就是记录vos中的EventFd
    dwRet = pSlaveDmm->create_mailbox(&pSlaveMbufer, MailboxAddr);
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }

    dwRet = pSlaveVos->VOS_RegTaskEventFd(VOS_TASK_SLAVE_MAILBOX, pSlaveMbufer->g_dwMbuferFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }

    dwRet = pSlaveVos->VOS_RegTaskFunc(VOS_TASK_SLAVE_MAILBOX, slave_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    /* 备机注册定时器添加到vos */
    timer *pRegisterTimer = new timer;
    dwRet = pRegisterTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error("pRegisterTimer->init error!");
        return FAILE;
    }
    
    //log_debug("pRegisterTimer->g_dwTimerFd(%lu)", pRegisterTimer->g_dwTimerFd);
    pRegisterTimer->get(&dwRet);
    log_debug("pRegisterTimer->get(%lu)", dwRet);
    
    dwRet = pSlaveVos->VOS_RegTaskEventFd(VOS_TASK_SLAVE_REGISTER_TIMER, pRegisterTimer->g_dwTimerFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }

    dwRet = pSlaveVos->VOS_RegTaskFunc(VOS_TASK_SLAVE_REGISTER_TIMER, slave_registerTimerProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    dwRet = pRegisterTimer->start(1000*60*2);//2min
    if(dwRet != SUCCESS)
    {
        log_error("pRegisterTimer->start error!");
        return FAILE;
    }

    pRegisterTimer->get(&dwRet);
    log_debug("pRegisterTimer->get(%lu)", dwRet);

    pSlaveVos->VOS_EpollWait(); //while(1)!!!

    return dwRet;
}

