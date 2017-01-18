#include <netinet/in.h> //for sockaddr_in
#include <sys/socket.h> //for recvfrom
#include <arpa/inet.h> //for inet_addr
#include <pthread.h> //for pthread
#include <sys/epoll.h> //for epoll
#include <stdlib.h> //for malloc
#include <stdint.h> //for unit64_t
#include <unistd.h> //for read
#include <errno.h> //for errno
#include <string.h> //for memset strstr
#include <fcntl.h> //for open
#include "macro.h"
#include "log.h"
#include "timer.h"
#include "event.h"
#include "socket.h"
#include "sync.h"
#include "tool.h"
#include "instantList.h"
#include "waitedList.h"

int g_iMainEpollFd = 0;
int g_iMainEventFd = 0;
int g_iSyncEventFd = 0;

int reliable_sync_init(void)
{
    /* list init */
    int iRet = instantList_init();
    if(iRet < 0)
    {
        log_error("instantList_init error!");
        return -1;
    }
    iRet = waitedList_init();
    if(iRet < 0)
    {
        log_error("waitedList_init error!");
        return -1;
    }

    /* epoll create */
    g_iMainEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(g_iMainEpollFd < 0)
    {
        log_error("epoll create error(%d)!", g_iMainEpollFd);
        return -1;
    }

    /* eventfd init */
    g_iMainEventFd = event_init(0); //0 for event flag init value
    if(g_iMainEventFd < 0)
    {
        log_error("event_init error(%d)!", g_iMainEventFd);
        return -1;
    }
    g_iSyncEventFd = event_init(0); //0 for event flag init value
    if(g_iSyncEventFd < 0)
    {
        log_error("event_init error(%d)!", g_iSyncEventFd);
        return -1;
    }

    /* add g_iMainEventFd to g_iMainEpollFd */
    iRet = tool_add_event_to_epoll(g_iMainEpollFd, g_iMainEventFd);
    if(iRet < 0)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", g_iMainEpollFd, g_iMainEventFd, iRet);
        return -1;
    }

    /* sync pthread create, send iMainEventFd to sync thread */
    pthread_t SyncThreadId;
    struct sync_struct stSyncStruct;
    iRet = pthread_create(&SyncThreadId, NULL, slave_sync_thread, (void *)&stSyncStruct);
    if(iRet != 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_mainEvent(void)
{
    log_info("Get iMainEventFd.");

    //获取事件标志，共有64种事件，可同时触发多个
    uint64_t uiEventsFlag;
    int iRet = event_getEventFlags(g_iMainEventFd, &uiEventsFlag);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }

    if(uiEventsFlag & SLAVE_MAIN_EVENT_NEWCFG_INSTANT)
    {
        log_info("Get SLAVE_MAIN_EVENT_NEWCFG_INSTANT, instant new cfg num(%d).", instantList_getListSize());

        //TO DO: how to recover
    }

    if(uiEventsFlag & SLAVE_MAIN_EVENT_NEWCFG_WAITED)
    {
        log_info("Get SLAVE_MAIN_EVENT_NEWCFG_WAITED, waited new cfg num(%d).", waitedList_getListSize());

        //TO DO: how to recover
    }

    if(uiEventsFlag & SLAVE_MAIN_EVENT_MASTER_RESTART) //when find specifyID different, get slave restart event
    {
        log_info("Get SLAVE_EVENT_MASTER_RESTART, batch backup.");

        //TO DO: clean the queue and batch backup
    }

    if(uiEventsFlag & SLAVE_MAIN_EVENT_CHECKALIVE_TIMER) //when not recived msg for a while, get check alive timer event
    {
        log_info("Get SLAVE_EVENT_CHECKALIVE_TIMER, restart.");

        /* restart */
    }

}

/*
 * 配置模块为实际主线程
 */
int main(int argc, char *argv[])
{
    //现用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_init();

    log_info("MAIN Task Beginning.");

    int iRet = reliable_sync_init();
    if(iRet < 0)
    {
        log_error("reliable_sync_init");
    }

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(1)
    {
        /* epoll wail */
        int iEpollNum = epoll_wait(g_iMainEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == g_iMainEventFd && stEvents[i].events & EPOLLIN)
            {
                iRet = _epoll_mainEvent();
                if(iRet < 0)
                {
                    log_warning("_epoll_mainEvent failed!");
                }
            }
        }//for
    }//while

    /* free all */
    log_info("MAIN Task Ending.");
    log_free();
    close(g_iMainEpollFd);
    close(g_iMainEventFd);
    return 0;
}


