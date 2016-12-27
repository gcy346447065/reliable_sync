#include <netinet/in.h> //for sockaddr_in htons
#include <arpa/inet.h> //for inet_addr
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include <errno.h> //for errno
#include <unistd.h> //for read write
#include <stdlib.h> //for malloc rand
#include <time.h> //for time
#include "sync.h"
#include "macro.h"
#include "log.h"
#include "socket.h"
#include "event.h"
#include "timer.h"
#include "queue.h"
#include "checksum.h"
#include "send.h"
#include "recv.h"
#include "protocol.h"

char g_cMasterSyncStatus = STATUS_INIT;
char g_cLoginRspSeq = 0;
char g_cMasterSpecifyID = 0;
char g_cSlaveSpecifyID = 0;

int g_iNewCfgID = 0;
int g_iKeepaliveTimerFd = 0;
int g_iSyncSockFd = 0;
int g_iLoginTimerFd = 0;

stQueue *g_pstInstantQueue;
stQueue *g_pstWaitedQueue;

int _add_to_epoll(struct epoll_event stEvent, int iSyncEpollFd, int iAddFd)
{
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iAddFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    return epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iAddFd, &stEvent);
}

void *master_sync(void *arg)
{
    log_info("SYNC task Beginning.");

    /*
     parse sync struct from main thread 
     */
    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;
    int iMainEventFd = pstSyncStruct->iMainEventFd;
    int iSyncEventFd = pstSyncStruct->iSyncEventFd;
    g_pstInstantQueue = pstSyncStruct->pstInstantQueue;
    g_pstWaitedQueue = pstSyncStruct->pstWaitedQueue;
    log_debug("iMainEventFd(%d), iSyncEventFd(%d)", iMainEventFd, iSyncEventFd);

    /* 
     epoll create 
     */
    int iSyncEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iSyncEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iSyncEpollFd);
        return (void *)-1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* 
     add sync eventfd to epoll 
     */
    int iRet = _add_to_epoll(stEvent, iSyncEpollFd, iSyncEventFd);
    if(iRet < 0)
    {
        log_error("epoll add iSyncEventFd error(%d)!", iRet);
        return (void *)-1;
    }

    /*
     set g_iSyncSockFd bind and connect, add g_iSyncSockFd to epoll 
     */
    g_iSyncSockFd = socket_init();
    if(g_iSyncSockFd < 0)
    {
        log_error("socket init error(%d)!", g_iSyncSockFd);
        return (void *)-1;
    }

    struct sockaddr_in stMasterSync2SyncAddr;
    memset(&stMasterSync2SyncAddr, 0, sizeof(stMasterSync2SyncAddr)); 
    stMasterSync2SyncAddr.sin_family = AF_INET;
    stMasterSync2SyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSync2SyncAddr.sin_port = htons(MASTER_SYNC_TO_SYNC_PORT);
    if(bind(g_iSyncSockFd, (struct sockaddr *)&stMasterSync2SyncAddr, sizeof(stMasterSync2SyncAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return (void *)-1;
    }

    struct sockaddr_in stSlaveSync2SyncAddr;
    memset(&stSlaveSync2SyncAddr, 0, sizeof(stSlaveSync2SyncAddr)); 
    stSlaveSync2SyncAddr.sin_family = AF_INET;
    stSlaveSync2SyncAddr.sin_addr.s_addr = inet_addr(SLAVE_IP); 
    stSlaveSync2SyncAddr.sin_port = htons(SLAVE_SYNC_TO_SYNC_PORT);
    if(connect(g_iSyncSockFd, (struct sockaddr *)&stSlaveSync2SyncAddr, sizeof(stSlaveSync2SyncAddr)) < 0)
    {
        log_error("socket connect error(%d)!", errno);
        return (void *)-1;
    }

    iRet = _add_to_epoll(stEvent, iSyncEpollFd, g_iSyncSockFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iSyncSockFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* 
     add master sync login timerfd to epoll 
     */
    g_iLoginTimerFd = timer_create();
    if(g_iLoginTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iLoginTimerFd);
        return (void *)-1;
    }

    iRet = _add_to_epoll(stEvent, iSyncEpollFd, g_iLoginTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iLoginTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    srand((int)time(0));
    g_cMasterSpecifyID = (char)(rand() % 0x100);

    /*
     add g_iKeepaliveTimerFd timerfd to epoll
     */
    g_iKeepaliveTimerFd = timer_create();
    if(g_iKeepaliveTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iKeepaliveTimerFd);
        return (void *)-1;
    }
    iRet = timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return (void *)-1;
    }
    iRet = _add_to_epoll(stEvent, iSyncEpollFd, g_iKeepaliveTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iKeepaliveTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    /*
     add iCheckaliveTimerFd timerfd to epoll 
     */
    int iCheckaliveTimerFd = timer_create();
    if(iCheckaliveTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", iCheckaliveTimerFd);
        return (void *)-1;
    }

    iRet = timer_start(iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return (void *)-1;
    }

    iRet = _add_to_epoll(stEvent, iSyncEpollFd, iCheckaliveTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add iCheckaliveTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    /*
     new cfg resend table init 
     */
    

    /* memset buffer and struct */
    int iBufferSize;
    char acBuffer[MAX_BUFFER_SIZE];
    memset(acBuffer, 0, MAX_BUFFER_SIZE);

    void *pMsg;
    int iMsgLen;

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

                if(uiEventsFlag & MASTER_EVENT_NEWCFG_INSTANT) 
                {
                    log_info("Get MASTER_EVENT_NEWCFG_INSTANT.");

                    if(queue_getReadSize(g_pstInstantQueue) != 0 && g_cMasterSyncStatus == STATUS_NEWCFG)
                    {
                        /* handle g_pstInstantQueue, send to slave */
                        log_debug("queue_getReadSize(%d).", queue_getReadSize(g_pstInstantQueue));

                        iRet = alloc_master_newCfgInstantReq(g_pstInstantQueue->pReadNow->pData, g_pstInstantQueue->pReadNow->iDataLen, g_pstInstantQueue->pReadNow->iDataID, &pMsg, &iMsgLen);
                        if(iRet < 0)
                        {
                            log_error("alloc_master_newCfgInstantReq error(%d)!", iRet);
                            return (void *)-1;
                        }

                        if(send2SlaveSync(g_iSyncSockFd, pMsg, iMsgLen) < 0)
                        {
                            log_debug("Send to SLAVE SYNC failed!");
                        }

                        /* queue_read g_pstInstantQueue */
                        queue_read(g_pstInstantQueue);
                        log_debug("queue_getReadSize(%d).", queue_getReadSize(g_pstInstantQueue));
                    }
                }

                if(uiEventsFlag & MASTER_EVENT_NEWCFG_WAITED) 
                {
                    log_info("Get MASTER_EVENT_NEWCFG_WAITED.");

                    if(queue_getReadSize(g_pstWaitedQueue) != 0 && g_cMasterSyncStatus == STATUS_NEWCFG)
                    {
                        /* handle g_pstWaitedQueue, send to slave */
                        log_debug("queue_getReadSize(%d).", queue_getReadSize(g_pstWaitedQueue));

                        iRet = alloc_master_newCfgWaitedReq(MAX_PKG_LEN, &pMsg, &iMsgLen);
                        if(iRet < 0)
                        {
                            log_error("alloc_master_newCfgWaitedReq error(%d)!", iRet);
                            return (void *)-1;
                        }
                        MSG_NEWCFG_WAITED_REQ *req = (MSG_NEWCFG_WAITED_REQ *)pMsg;

                        int i=0, iMsgLenSum = sizeof(MSG_NEWCFG_WAITED_REQ);
                        DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);
                        while(queue_getReadSize(g_pstWaitedQueue) != 0)
                        {
                            iMsgLenSum = iMsgLenSum + sizeof(DATA_NEWCFG) + g_pstWaitedQueue->pReadNow->iDataLen;
                            if(iMsgLenSum >= MAX_PKG_LEN)
                            {
                                iMsgLenSum = iMsgLenSum - sizeof(DATA_NEWCFG) - g_pstWaitedQueue->pReadNow->iDataLen;
                                break;
                            }

                            pDataNewcfg->iNewCfgID = htonl(g_pstWaitedQueue->pReadNow->iDataID);
                            pDataNewcfg->sChecksum = htons(checksum((const char *)g_pstWaitedQueue->pReadNow->pData, g_pstWaitedQueue->pReadNow->iDataLen));
                            pDataNewcfg->iDataLen = htonl(g_pstWaitedQueue->pReadNow->iDataLen);
                            memcpy(pDataNewcfg->acData, g_pstWaitedQueue->pReadNow->pData, g_pstWaitedQueue->pReadNow->iDataLen);

                            queue_read(g_pstWaitedQueue);
                            i++;
                            pDataNewcfg++;
                        }

                        req->msgHeader.sLength = htons(iMsgLenSum - MSG_HEADER_LEN);

                        const char *pcData = (const char *)req->dataNewcfg;
                        req->sAllChecksum = htons(checksum(pcData, iMsgLenSum - sizeof(MSG_NEWCFG_WAITED_REQ)));

                        if(send2SlaveSync(g_iSyncSockFd, pMsg, iMsgLenSum) < 0)
                        {
                            log_debug("Send to SLAVE SYNC failed!");
                        }

                        log_debug("queue_getReadSize(%d).", queue_getReadSize(g_pstInstantQueue));
                    }
                }
            }//if iSyncEventFd
            else if(stEvents[i].data.fd == g_iLoginTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iLoginTimerFd.");
                if(g_cMasterSyncStatus == STATUS_LOGIN)
                {
                    //send login msg
                    MSG_LOGIN_RSP *rsp;
                    iRet = alloc_master_rspMsg(CMD_LOGIN, g_cLoginRspSeq, (void **)&rsp);
                    if(iRet < 0)
                    {
                        log_error("alloc_master_rspMsg error(%d)!", iRet);
                        return (void *)-1;
                    }
                    rsp->cSynAckFlag = 1;//the second one in three-way handshake
                    rsp->cSpecifyID = g_cMasterSpecifyID;
                    if(send2SlaveSync(g_iSyncSockFd, rsp, sizeof(MSG_LOGIN_RSP)) < 0)
                    {
                        log_debug("Send to SLAVE SYNC failed!");
                    }
                }

                iRet = timer_start(g_iLoginTimerFd, LOGIN_TIMER_VALUE); //8s
                if(iRet < 0)
                {
                    log_error("login timer start error(%d)!", iRet);
                    return (void *)-1;
                }
            }//else if g_iLoginTimerFd
            else if(stEvents[i].data.fd == g_iKeepaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iKeepaliveTimerFd.");

                //no msg sent for a while, send keep alive msg
                MSG_KEEP_ALIVE_REQ *req;
                iRet = alloc_master_reqMsg(CMD_KEEP_ALIVE, (void **)&req);
                if(iRet < 0)
                {
                    log_error("alloc_master_reqMsg error(%d)!", iRet);
                    return (void *)-1;
                }

                if(send2SlaveSync(g_iSyncSockFd, req, sizeof(MSG_KEEP_ALIVE_REQ)) < 0)
                {
                    log_debug("Send keepalive req to SLAVE SYNC failed! To send again!");

                    //TODO :send for 5 timers
                }

            }//else if g_iKeepaliveTimerFd
            else if(stEvents[i].data.fd == iCheckaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iCheckaliveTimerFd.");

                //no msg recived for a while, send event to main to restart
                iRet = event_setEventFlags(iMainEventFd, MASTER_EVENT_CHECKALIVE_TIMER);
                if(iRet < 0)
                {
                    log_error("set iMainEventFd MASTER_EVENT_CHECKALIVE_TIMER error(%d)!", iRet);
                    return (void *)-1;
                }

            }//else if iCheckaliveTimerFd
            else if(stEvents[i].data.fd == g_iSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iSyncSockFd.");

                /* wait for socket msg */
                memset(acBuffer, 0, MAX_BUFFER_SIZE);
                if((iBufferSize = read(g_iSyncSockFd, acBuffer, MAX_BUFFER_SIZE)) > 0)
                {
                    log_hex(acBuffer, iBufferSize);
                    handle_sync_msg(acBuffer, iBufferSize);

                    //restart keep alive timer
                    iRet = timer_start(iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);
                    if(iRet < 0)
                    {
                        log_error("sync timer start error(%d)!", iRet);
                        return (void *)-1;
                    }
                }
            }//else if g_iSyncSockFd
        }//for
    }//while

    return (void *)0;
}



