#include <netinet/in.h> //for sockaddr_in
#include <sys/socket.h> //for recvfrom
#include <arpa/inet.h> //for inet_addr
#include <pthread.h> //for pthread
#include <sys/epoll.h> //for epoll
#include <stdlib.h> //for malloc
#include <stdint.h> //for unit64_t
#include <stdio.h> //for sscanf sprintf
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
#include "queue.h"

enum SEND_METHOD_TO_QUEUE
{
    SEND_NEWCFG_WAITED,
    SEND_NEWCFG_INSTANT
};

int send2Queue(stQueue *pstQueue, void *pBuf, int iBufLen, int iMaxPkgLen, void *pDestAddr, int iSendMethod)
{
    if(pBuf == NULL || iBufLen == 0)
    {
        log_error("SendToSync pBuf or iBufLen error!");
        return -1;
    }
    //TODO: iMaxPkgLen, pDestAddr unused

    switch(iSendMethod)
    {
        case SEND_NEWCFG_WAITED:
            log_info("SEND_NEWCFG_WAITED.");
            queue_push(pstQueue, pBuf, iBufLen);//pstWaitedQueue
            break;

        case SEND_NEWCFG_INSTANT:
            log_info("SEND_NEWCFG_INSTANT.");
            queue_push(pstQueue, pBuf, iBufLen);//pstInstantQueue
            break;
            
        default:
            log_error("iSendMethod:%d.", iSendMethod);
            return -1;
    }

    log_info("SendToSync ok.");
    return 0;
}

int main(int argc, char *argv[])
{
    /* log init */
    log_init();

    /* queue init */
    stQueue *pstInstantQueue = queue_init();
    if(pstInstantQueue == NULL )
    {
        log_error("queue_init error!");
        return -1;
    }
    stQueue *pstWaitedQueue = queue_init();
    if(pstWaitedQueue == NULL )
    {
        log_error("queue_init error!");
        return -1;
    }

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

    /* add STDIN_FILENO to epoll for test(to trigger master find new config) */
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = STDIN_FILENO;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iMainEpollFd, EPOLL_CTL_ADD, STDIN_FILENO, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add STDIN_FILENO error(%d)!", iRet);
        return -1;
    }

    /* sync pthread create, send iMainEventFd to sync thread */
    pthread_t SyncThreadId;
    struct sync_struct stSyncStruct;
    stSyncStruct.iMainEventFd = iMainEventFd;
    stSyncStruct.iSyncEventFd = iSyncEventFd;
    stSyncStruct.pstInstantQueue = pstInstantQueue;
    stSyncStruct.pstWaitedQueue = pstWaitedQueue;
    iRet = pthread_create(&SyncThreadId, NULL, master_sync, (void *)&stSyncStruct);
    if(iRet != 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    /* log for Beginning */
    log_info("Main Task Beginning.");

    int iNewCfgFd, iFileBegin, iFileEnd;
    char acFilenameBegin[128], acFilenameEnd[128];
    while(1)
    {
        /* main task */

        /* epoll wail */
        int iEpollNum = epoll_wait(iMainEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == STDIN_FILENO && stEvents[i].events & EPOLLIN)
            {
                log_info("Get STDIN_FILENO fd.");

                /* read STDIN_FILENO */
                char acStdinBuf[128];
                memset(acStdinBuf, 0, 128);

                iRet = read(STDIN_FILENO, acStdinBuf, 128);
                if(iRet < 0)
                {
                    log_error("read STDIN_FILENO error(%s)!", strerror(errno));
                    return -1;
                }
                acStdinBuf[iRet-1] = '\0'; //-1 for '\n'

                if(sscanf(acStdinBuf, "?file%d", &iFileBegin) == 1) //"?file2"
                {
                    sprintf(acFilenameBegin, "file%d", iFileBegin);
                    if((iNewCfgFd = open(acFilenameBegin, O_RDONLY)) > 0)
                    {
                        log_info("open INSTANT file ok.");

                        iRet = write(STDIN_FILENO, "Send INSTANT to slave.\r\n", strlen("Send INSTANT to slave.\r\n"));
                        if(iRet < 0)
                        {
                            log_error("write STDIN_FILENO error(%s)!", strerror(errno));
                            return -1;
                        }

                        iRet = event_setEventFlags(iMainEventFd, MASTER_EVENT_KEYIN_INSTANT);
                        if(iRet != 0)
                        {
                            log_info("set event flag MASTER_EVENT_KEYIN_INSTANT failed!");
                        }
                    }
                    else
                    {
                        log_error("open new config file error(%s)!", strerror(errno));
                    }
                }
                else if(sscanf(acStdinBuf, "/file%d:file%d", &iFileBegin, &iFileEnd) == 2) //"/file8:file10"
                {
                    sprintf(acFilenameBegin, "file%d", iFileBegin);
                    sprintf(acFilenameEnd, "file%d", iFileEnd);
                    if(open(acFilenameBegin, O_RDONLY) > 0 && open(acFilenameEnd, O_RDONLY) > 0)
                    {
                        log_info("open WAITED files ok.");

                        iRet = write(STDIN_FILENO, "Send WAITED to slave.\r\n", strlen("Send WAITED to slave.\r\n"));
                        if(iRet < 0)
                        {
                            log_error("write STDIN_FILENO error(%s)!", strerror(errno));
                            return -1;
                        }

                        iRet = event_setEventFlags(iMainEventFd, MASTER_EVENT_KEYIN_WAITED);
                        if(iRet != 0)
                        {
                            log_info("set event flag MASTER_EVENT_KEYIN_WAITED failed!");
                        }
                    }
                    else
                    {
                        log_error("open new config file error(%s)!", strerror(errno));
                    }
                }
                else
                {
                    log_error("Unknown command!");
                }
            }//if STDIN_FILENO
            else if(stEvents[i].data.fd == iMainEventFd && stEvents[i].events & EPOLLIN)
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

                if(uiEventsFlag & MASTER_EVENT_KEYIN_INSTANT) //planned: set by main mask, tested: when get STDIN_FILENO keys in
                {
                    log_info("Get MASTER_EVENT_KEYIN_INSTANT.");

                    /* realtime SendToSync */
                    char *pcNewCfgBuf = (char *)malloc(MAX_PKG_LEN);
                    memset(pcNewCfgBuf, 0, MAX_PKG_LEN);
                    iRet = read(iNewCfgFd, pcNewCfgBuf, MAX_PKG_LEN);
                    if(iRet < 0)
                    {
                        log_error("read iNewCfgFd error(%d)!", iRet);
                        return -1;
                    }
                    int iBufLen = iRet;

                    iRet = send2Queue(pstInstantQueue, pcNewCfgBuf, iBufLen, MAX_PKG_LEN, NULL, SEND_NEWCFG_INSTANT);
                    if(iRet < 0)
                    {
                        log_error("send2Queue error(%d)!", iRet);
                    }

                    event_setEventFlags(iSyncEventFd, MASTER_EVENT_NEWCFG_INSTANT);
                }

                if(uiEventsFlag & MASTER_EVENT_KEYIN_WAITED) //when find specifyID different, get slave restart event
                {
                    log_info("Get MASTER_EVENT_KEYIN_WAITED.");

                    /* loop to send to waited queue */
                    char *pcNewCfgBuf = (char *)malloc(MAX_PKG_LEN);
                    for(int i=iFileBegin; i<=iFileEnd; i++)
                    {
                        memset(pcNewCfgBuf, 0, MAX_PKG_LEN);
                        sprintf(acFilenameBegin, "file%d", i);
                        if((iNewCfgFd = open(acFilenameBegin, O_RDONLY)) > 0)
                        {
                            iRet = read(iNewCfgFd, pcNewCfgBuf, MAX_PKG_LEN);
                            if(iRet < 0)
                            {
                                log_error("read iNewCfgFd error(%d)!", iRet);
                                return -1;
                            }
                            int iBufLen = iRet;

                            iRet = send2Queue(pstWaitedQueue, pcNewCfgBuf, iBufLen, MAX_PKG_LEN, NULL, SEND_NEWCFG_WAITED);
                            if(iRet < 0)
                            {
                                log_error("send2Queue error(%d)!", iRet);
                            }
                        }
                    }

                    event_setEventFlags(iSyncEventFd, MASTER_EVENT_NEWCFG_WAITED);
                }

                if(uiEventsFlag & MASTER_EVENT_SLAVE_RESTART) //when find specifyID different, get slave restart event
                {
                    log_info("Get MASTER_EVENT_SLAVE_RESTART, batch backup.");

                    /* batch backup */
                }

                if(uiEventsFlag & MASTER_EVENT_CHECKALIVE_TIMER) //when not recived msg for a while, get check alive timer event
                {
                    log_info("Get MASTER_EVENT_CHECKALIVE_TIMER, restart.");

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

