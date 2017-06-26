#include <pthread.h> //for pthread
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset
#include <stdlib.h> //for malloc
#include <unistd.h> //for read
#include "sync_master.h"
#include "mbufer.h"
#include "timer.h"
#include "list_instant.h"
#include "list_waited.h"
#include "log.h"

struct sync_master_struct
{
    DWORD dwLclAddr;
    DWORD dwOppAddr;
};

extern event *g_pSyncEvent;

dmm* g_pDmm;
mbufer* g_pMailbox;
timer *g_pKeepaliveTimer;


static DWORD __epoll_addFd(DWORD dwEpollFd, DWORD dwEventFd)
{
    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = dwEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered

    DWORD dwRet = epoll_ctl(dwEpollFd, EPOLL_CTL_ADD, dwEventFd, &stEvent);
    return dwRet;
}

static DWORD __epoll_mbufer(void)
{
    log_info("__epoll_mbufer");

    INT iBufferSize = 0;
    CHAR *pcBuffer = (CHAR *)malloc(MAX_BUFFER_SIZE);
    memset(pcBuffer, 0, MAX_BUFFER_SIZE);
    if((iBufferSize = read(g_pMbufer->g_dwMbuferFd, pcBuffer, MAX_BUFFER_SIZE)) > 0)
    {
        log_hex(pcBuffer, iBufferSize);
    }

    free(pcBuffer);

    return SUCCESS;
}

static DWORD __epoll_syncEvent(void)
{
    log_info("__epoll_syncEvent");

    //获取事件标志，共有64种事件，可同时触发多个
    QWORD qwEventsFlag;
    DWORD dwRet = g_pSyncEvent->getEventFlags(&qwEventsFlag);
    if(dwRet != SUCCESS)
    {
        log_error("event_getEventFlags error(%d)!", dwRet);
        return FAILE;
    }

    if(qwEventsFlag & MASTER_SYNC_EVENT_NEWCFG_INSTANT)
    {
        log_info("Get MASTER_SYNC_EVENT_NEWCFG_INSTANT.");

        log_info("hehe.");
        /* restart */
    }

    if(qwEventsFlag & MASTER_SYNC_EVENT_NEWCFG_WAITED)
    {
        log_info("Get MASTER_SYNC_EVENT_NEWCFG_WAITED.");

        /* restart */
    }

    return 0;
}

static DWORD __epoll_keepaliveTimer(void)
{
    log_info("__epoll_keepaliveTimer");

    DWORD dwRet = g_pKeepaliveTimer->start(KEEPALIVE_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("keep alive timer_start error(%lu)!", dwRet);
        return FAILE;
    }

    return SUCCESS;
}

/*
 * sync模块的起点，由main线程创建
 */
void *sync_master_thread(void *arg)
{
    log_info("SYNC Task Beginning.");

    struct sync_master_struct *pstSyncStruct = (struct sync_master_struct *)arg;
    DWORD dwLclAddr = pstSyncStruct->dwLclAddr;
    DWORD dwOppAddr = pstSyncStruct->dwOppAddr;

    /* epoll调用 */
    DWORD dwSyncEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(dwSyncEpollFd < 0)
    {
        log_error("epoll create error(%d)!", dwSyncEpollFd);
        return (void *)FAILE;
    }

    g_pDmm = new dmm;
    g_pMailbox = new mbufer;

    
    DWORD dwRet = g_pDmm->create_mailbox(dwLclAddr, dwOppAddr);
    if(dwRet != SUCCESS)
    {
        log_error("g_pMbufer init error(%lu)!", dwRet);
        return (void *)FAILE;
    }

    dwRet = __epoll_addFd(dwSyncEpollFd, g_pMailbox->g_dwMbuferFd);
    if(dwRet != SUCCESS)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", dwSyncEpollFd, g_pMbufer->g_dwMbuferFd, dwRet);
        return (void *)FAILE;
    }

    g_pKeepaliveTimer = new timer;
    dwRet = g_pKeepaliveTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error("g_pKeepaliveTimer init error(%lu)!", dwRet);
        return (void *)FAILE;
    }

    dwRet = __epoll_addFd(dwSyncEpollFd, g_pKeepaliveTimer->g_dwTimerFd);
    if(dwRet != SUCCESS)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", dwSyncEpollFd, g_pKeepaliveTimer->g_dwTimerFd, dwRet);
        return (void *)FAILE;
    }

    /*dwRet = g_pKeepaliveTimer->start(KEEPALIVE_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("keep alive timer_start error(%lu)!", dwRet);
        return (void *)FAILE;
    }*/

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(1)
    {
        INT iEpollNum = epoll_wait(dwSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(INT i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == g_pMbufer->g_dwMbuferFd && stEvents[i].events & EPOLLIN)
            {
                dwRet = __epoll_mbufer();
                if(dwRet != SUCCESS)
                {
                    log_warning("__epoll_mbufer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_pSyncEvent->g_dwEventFd && stEvents[i].events & EPOLLIN)
            {
                dwRet = __epoll_syncEvent();
                if(dwRet != SUCCESS)
                {
                    log_warning("__epoll_syncEvent failed!");
                }
            }
            else if(stEvents[i].data.fd == g_pKeepaliveTimer->g_dwTimerFd && stEvents[i].events & EPOLLIN)
            {
                dwRet = __epoll_keepaliveTimer();
                if(dwRet != SUCCESS)
                {
                    log_warning("__epoll_keepaliveTimer failed!");
                }
            }
            
        }
    }

    log_info("SYNC Task Ending.");
    return (void *)SUCCESS;
}

/*
 * 发送端，但可能发送到多个地址
 * 每需要一条同步通道则实例化一次
 */
DWORD sync_master::init(DWORD dwLclAddr, DWORD dwOppAddr, DWORD dwTimeThr, DWORD dwQttyThr)
{
    //开始login流程变为STATUS_LOGIN，login成功后变为STATUS_NEWCFG，才会开始发送配置数据
    g_cMasterStatus = STATUS_INIT;

    /* 链表初始化 */
    list_instant *pListInstant = new list_instant;
    DWORD dwRet = pListInstant->init();
    if(dwRet != SUCCESS)
    {
        log_error("sync_slave init error!");
        return FAILE;
    }

    /* 创建master_sync_thread */
    pthread_t ThreadId;
    struct sync_master_struct stSyncStruct;
    stSyncStruct.dwLclAddr = dwLclAddr;
    stSyncStruct.dwOppAddr = dwOppAddr;
    if(pthread_create(&ThreadId, NULL, sync_master_thread, (void *)&stSyncStruct) < 0)
    {
        log_error("pthread create error!");
        return FAILE;
    }
    

    return SUCCESS;
}

/*
 功能：
 将配置数据放入instant链表
 放入一个则发送一个，回复成功则删节点，回复失败则重发
 
 入参：
 void *pBuf 配置数据指针
 DWORD dwBufLen 配置数据长度
 DWORD dwOrderRange 当前配置所属的顺序范围，如果为0则说明当前配置不需要顺序还原
 DWORD dwOrderNum 当前配置所属的顺序；比如上个范围值为30时，此值可取1到30

 出参：
 无
 */
DWORD sync_master::sendInstant(void *pBuf, DWORD dwBufLen, DWORD dwOrderRange, DWORD dwOrderNum)
{
    return SUCCESS;
}

/*
 * 将配置数据放入waited链表
 * 满足定时定量阈值才会打包发送，
 */
DWORD sync_master::sendWaited(void *pBuf, DWORD dwBufLen, DWORD dwOrderRange, DWORD dwOrderNum)
{
    return SUCCESS;
}
