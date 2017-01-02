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


int main(int argc, char *argv[])
{
    /* log init */
    log_init();

    /* epoll create */
    int iMainEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iMainEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iMainEpollFd);
        return -1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* main eventfd init, add main eventfd to epoll */
    int iMainEventFd = event_init(0); //0 for event flag init value
    if(iMainEventFd < 0)
    {
        log_error("iMainEventFd error(%d)!", iMainEventFd);
        return -1;
    }
    log_debug("iMainEventFd(%d)", iMainEventFd);
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iMainEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    int iRet = epoll_ctl(iMainEpollFd, EPOLL_CTL_ADD, iMainEventFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iMainEventFd error(%d)!", iRet);
        return -1;
    }

    /* sync eventfd init, for sync read */
    int iSyncEventFd = event_init(0); //0 for event flag init value
    if(iSyncEventFd < 0)
    {
        log_error("iSyncEventFd error(%d)!", iSyncEventFd);
        return -1;
    }

    /* sync pthread create, send iMainEventFd to sync thread */
    pthread_t SyncThreadId;
    struct sync_struct stSyncStruct;
    stSyncStruct.iMainEventFd = iMainEventFd;
    stSyncStruct.iSyncEventFd = iSyncEventFd;
    iRet = pthread_create(&SyncThreadId, NULL, slave_sync, (void *)&stSyncStruct);
    if(iRet != 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    /* log for Beginning */
    log_info("Main Task Beginning.");

    while(1)
    {
        /* main task */

        /* epoll wail */
        int iEpollNum = epoll_wait(iMainEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == iMainEventFd && stEvents[i].events & EPOLLIN)
            {
                log_info("Get iMainEventFd.");

                /* get events from iMainEventFd */
                uint64_t uiEventsFlag;
                iRet = event_getEventFlags(iMainEventFd, &uiEventsFlag);
                if(iRet < 0)
                {
                    log_error("event_getEventFlags error(%d)!", iRet);
                    return -1;
                }

                if(uiEventsFlag & SLAVE_EVENT_MASTER_RESTART) //when find specifyID different, get slave restart event
                {
                    log_info("Get SLAVE_EVENT_MASTER_RESTART, batch backup.");

                    //TO DO: clean the queue and batch backup
                }

                if(uiEventsFlag & SLAVE_EVENT_CHECKALIVE_TIMER) //when not recived msg for a while, get check alive timer event
                {
                    log_info("Get SLAVE_EVENT_CHECKALIVE_TIMER, restart.");

                    /* restart */
                }
            }//else if iMainEventFd
        }//for
    }//while

    /* free all */
    log_info("Main Task Ending.");
    log_free();
    close(iMainEpollFd);
    close(iMainEventFd);
    return 0;
}


