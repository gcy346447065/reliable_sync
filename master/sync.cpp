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



void *master_sync(void *arg)
{
    log_info("SYNC task Beginning.");

    /* parse sync struct from main thread */
    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;
    int iMainEventFd = pstSyncStruct->iMainEventFd;
    log_debug("iMainEventFd = %d", iMainEventFd); //TO DO:set eventfd flag

    /* epoll create */
    int iSyncEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iSyncEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iSyncEpollFd);
        return (void *)-1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* add sync eventfd to epoll */
    int iSyncEventFd = event_init(0); //0 for event flag init value
    if(iSyncEventFd < 0)
    {
        log_error("iSyncEventFd error(%d)!", iSyncEventFd);
        return (void *)-1;
    }
    log_debug("iSyncEventFd(%d)", iSyncEventFd);
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    int iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iSyncEventFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSyncEventFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* set iSyncToMainSockFd bind and connect, add iSyncToMainSockFd to epoll */
    int iSyncToMainSockFd = socket_init();
    if(iSyncToMainSockFd < 0)
    {
        log_error("socket init error(%d)!", iSyncToMainSockFd);
        return (void *)-1;
    }
    log_debug("iSyncToMainSockFd(%d).", iSyncToMainSockFd);

    struct sockaddr_in stMasterSyncAddr;
    memset(&stMasterSyncAddr, 0, sizeof(stMasterSyncAddr)); 
    stMasterSyncAddr.sin_family = AF_INET; 
    stMasterSyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSyncAddr.sin_port = htons(MASTER_SYNC_PORT);
    if(bind(iSyncToMainSockFd, (struct sockaddr*)&stMasterSyncAddr, sizeof(stMasterSyncAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return (void *)-1;
    }

    struct sockaddr_in stMasterMainAddr;
    memset(&stMasterMainAddr, 0, sizeof(stMasterMainAddr)); 
    stMasterMainAddr.sin_family = AF_INET; 
    stMasterMainAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterMainAddr.sin_port = htons(MASTER_MAIN_PORT);
    if(connect(iSyncToMainSockFd, (struct sockaddr*)&stMasterMainAddr, sizeof(stMasterMainAddr)) < 0)
    {
        log_error("socket connect error(%d)!", errno);
        return (void *)-1;
    }

    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncToMainSockFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iSyncToMainSockFd, &stEvent);
    if(iRet < 0)
    {
        log_error("iSyncEpollFd add iSyncToMainSockFd error(%d)!", iRet);
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

    struct sockaddr_in stMasterSyncForSyncAddr;
    memset(&stMasterSyncForSyncAddr, 0, sizeof(stMasterSyncForSyncAddr)); 
    stMasterSyncForSyncAddr.sin_family = AF_INET;
    stMasterSyncForSyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSyncForSyncAddr.sin_port = htons(MASTER_SYNC_FOR_SYNC_PORT);
    if(bind(iSyncToSyncSockFd, (struct sockaddr *)&stMasterSyncForSyncAddr, sizeof(stMasterSyncForSyncAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return (void *)-1;
    }

    struct sockaddr_in stSlaveSyncForSyncAddr;
    memset(&stSlaveSyncForSyncAddr, 0, sizeof(stSlaveSyncForSyncAddr)); 
    stSlaveSyncForSyncAddr.sin_family = AF_INET;
    stSlaveSyncForSyncAddr.sin_addr.s_addr = inet_addr(SLAVE_IP); 
    stSlaveSyncForSyncAddr.sin_port = htons(SLAVE_SYNC_FOR_SYNC_PORT);
    if(connect(iSyncToSyncSockFd, (struct sockaddr *)&stSlaveSyncForSyncAddr, sizeof(stSlaveSyncForSyncAddr)) < 0)
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

    /* add sync timerfd to epoll */
    int iSyncTimerFd = timer_create();
    if(iSyncTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", iSyncTimerFd);
        return (void *)-1;
    }
    iRet = timer_start(iSyncTimerFd, 1000 * 60 * 1); //planned: 10 min, tested: 1 min
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return (void *)-1;
    }
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncTimerFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iSyncTimerFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSyncTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* memset buffer and struct */
    char acBuffer[BUFFER_SIZE];
    memset(acBuffer, 0, BUFFER_SIZE);

    char acReadBuffer[BUFFER_SIZE];
    memset(acReadBuffer, 0, BUFFER_SIZE);

    struct sockaddr_in stRecvAddr;
    memset(&stRecvAddr, 0, sizeof(stRecvAddr));
    int iRecvAddrLen = 0;

    while(1)
    {
        /* epoll wail */
        int iEpollNum = epoll_wait(iSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == iSyncToMainSockFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iSyncToMainSockFd.");

                /* wait for socket msg */
                memset(acBuffer, 0, BUFFER_SIZE);
                if(read(iSyncToMainSockFd, acBuffer, BUFFER_SIZE) > 0)
                {
                    //TO DO: the stRecvAddr of the first msg is NULL
                    log_debug("Get socket msg: %s.", acBuffer);
                }

                /*if(sendto(iSyncSockFd, "SYN ACK", 7, 0, (struct sockaddr *)&stRecvAddr, iRecvAddrLen) < 0)
                {
                    log_debug("Send SYN ACK to client failed!");
                    g_bSendSynAckFlag = false;
                }*/
            }//if
            #if 0
            else if(stEvents[i].data.fd == iSyncEventFd && stEvents[i].events & EPOLLIN)
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

                if(uiEventsFlag & EVENT_FLAG_SLAVE_RESTART) //set the flag when sync thread find no keep alive ack
                {
                    log_info("Get slave restart event flag.");

                    /* batch SendToSync */

                    /* read timerfd to loop again */
                }

                if(uiEventsFlag & EVENT_FLAG_MASTER_NEWCFG) //planned: set by main thread, tested: when get STDIN_FILENO keys in
                {
                    log_info("Get master new config event flag.");

                    /* realtime SendToSync */
                }
            }//else if
            #endif
        }//for
    }//while

    return (void *)0;
}



