#include <netinet/in.h> //for sockaddr_in
#include <arpa/inet.h> //for inet_addr
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include <errno.h> //for errno
#include <unistd.h> //for read
#include "sync.h"
#include "macro.h"
#include "log.h"
#include "socket.h"
#include "event.h"
#include "timer.h"


void *slave_sync(void *arg)
{
    log_info("SYNC task Beginning.");

    /* parse sync struct from main thread */
    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;
    int iMainEventFd = pstSyncStruct->iMainEventFd;
    int iSyncEventFd = pstSyncStruct->iSyncEventFd;
    log_debug("iMainEventFd(%d), iSyncEventFd(%d)", iMainEventFd, iSyncEventFd);

    /* epoll create */
    int iSyncEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iSyncEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iSyncEpollFd);
        return (void *)-1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* add sync eventfd to epoll */
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    int iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iSyncEventFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSyncEventFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* set iSyncToSyncSockFd bind and connect, add iSyncToSyncSockFd to epoll */
    int iSyncToSyncSockFd = socket_init();
    if(iSyncToSyncSockFd < 0)
    {
        log_error("socket init error(%d)!", iSyncToSyncSockFd);
        return (void *)-1;
    }
    log_debug("iSyncToSyncSockFd(%d).", iSyncToSyncSockFd);

    struct sockaddr_in stSlaveSync2SyncAddr;
    memset(&stSlaveSync2SyncAddr, 0, sizeof(stSlaveSync2SyncAddr)); 
    stSlaveSync2SyncAddr.sin_family = AF_INET;
    stSlaveSync2SyncAddr.sin_addr.s_addr = inet_addr(SLAVE_IP); 
    stSlaveSync2SyncAddr.sin_port = htons(SLAVE_SYNC_TO_SYNC_PORT);
    if(bind(iSyncToSyncSockFd, (struct sockaddr *)&stSlaveSync2SyncAddr, sizeof(stSlaveSync2SyncAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return (void *)-1;
    }

    struct sockaddr_in stMasterSync2SyncAddr;
    memset(&stMasterSync2SyncAddr, 0, sizeof(stMasterSync2SyncAddr)); 
    stMasterSync2SyncAddr.sin_family = AF_INET;
    stMasterSync2SyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSync2SyncAddr.sin_port = htons(MASTER_SYNC_TO_SYNC_PORT);
    if(connect(iSyncToSyncSockFd, (struct sockaddr *)&stMasterSync2SyncAddr, sizeof(stMasterSync2SyncAddr)) < 0)
    {
        log_error("socket connect error(%d)!", errno);
        return (void *)-1;
    }

    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncToSyncSockFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for recvfrom, edge triggered
    iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iSyncToSyncSockFd, &stEvent);
    if(iRet < 0)
    {
        log_error("iSyncEpollFd add iSyncToSyncSockFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* memset buffer and struct */
    int iBufferSize;
    char acBuffer[MAX_BUFFER_SIZE];
    memset(acBuffer, 0, MAX_BUFFER_SIZE);

    while(1)
    {
        /* epoll wail */
        int iEpollNum = epoll_wait(iSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == iSyncEventFd && stEvents[i].events & EPOLLIN)
            {
                log_info("Get eventfd.");

                /* get events from iSyncEventFd */
                uint64_t uiEventsFlag;
                iRet = event_getEventFlags(iSyncEventFd, &uiEventsFlag);
                if(iRet < 0)
                {
                    log_error("event_getEventFlags error(%d)!", iRet);
                    return (void *)-1;
                }

                if(uiEventsFlag & EVENT_FLAG_QUEUE_PUSH) //set the flag when sync thread find no keep alive ack
                {
                    log_info("Get EVENT_FLAG_QUEUE_PUSH.");
                }

            }//if iSyncEventFd
            else if(stEvents[i].data.fd == iSyncToSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iSyncToSyncSockFd.");

                /* wait for socket msg */
                memset(acBuffer, 0, MAX_BUFFER_SIZE);
                if((iBufferSize = read(iSyncToSyncSockFd, acBuffer, MAX_BUFFER_SIZE)) > 0) //after connect, read = recv
                {
                    log_debug("Get socket msg from MASTER(%d):%s.", iBufferSize, acBuffer);

                    if(write(iSyncToSyncSockFd, "ack", 3) < 0) //after connect, write = send
                    {
                        log_error("Send to SLAVE SYNC failed!");
                    }
                }

            }//else if iSyncToSyncSockFd
        }//for
    }//while

    return (void *)0;
}



