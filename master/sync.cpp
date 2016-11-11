#include <netinet/in.h> //for sockaddr_in
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include "sync.h"
#include "macro.h"
#include "log.h"
#include "socket.h"
#include "event.h"

int g_iBackupFlag = BACKUP_NULL;
const void *g_pAddr = NULL;
int g_iLength = 0;

void *master_sync(void *arg)
{
    log_info("SYNC task Beginning.");

    /* parse sync struct from main thread */
    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;
    int iSyncEventFd = pstSyncStruct->iSyncEventFd;
    log_debug("iSyncEventFd = %d", iSyncEventFd);

    /* socket init */
    int iRet = socket_init();
    if(iRet < 0)
    {
        log_error("socket init error(%d)!", iRet);
        return (void *)-1;
    }
    int iSockFd = iRet;
    log_debug("Server iSockFd=%d.", iSockFd);

    /* epoll create */
    int iEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iEpollFd);
        return (void *)-1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* add iSyncEventFd to epoll */
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for recvfrom, edge triggered
    iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, iSyncEventFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSyncEventFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* add iSockFd to epoll */
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSockFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for recvfrom, edge triggered
    iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, iSockFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSockFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* timer create, start the specified timer when needs */


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
        int iEpollNum = epoll_wait(iEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == iSockFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iSockFd.");

                /* wait for socket msg */
                memset(acBuffer, 0, BUFFER_SIZE);
                int iRecvLen = recvfrom(iSockFd, acBuffer, BUFFER_SIZE, 0, (struct sockaddr *)&stRecvAddr, (socklen_t *)&iRecvAddrLen);
                if(iRecvLen > 0)
                {
                    //TO DO: the stRecvAddr of the first msg is NULL
                    log_debug("Get socket msg: %d, %s.", stRecvAddr.sin_port, acBuffer);
                }

                /*if(sendto(iSockFd, "SYN ACK", 7, 0, (struct sockaddr *)&stRecvAddr, iRecvAddrLen) < 0)
                {
                    log_debug("Send SYN ACK to client failed!");
                    g_bSendSynAckFlag = false;
                }*/
            }//if
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
        }//for
    }//while

    return (void *)0;
}


int SetBackupFlag(int iType, const void *pAddr, int iLength)
{
    g_iBackupFlag = iType;
    g_pAddr = pAddr;
    g_iLength = iLength;
    
    return 0;
}

int GetBackupFlag(int *piType, const void **ppAddr, int *piLength)
{
    *piType = g_iBackupFlag;
    *ppAddr = g_pAddr;
    *piLength = g_iLength;
    
    return 0;
}

int ResetBackupFlag(void)
{
    g_iBackupFlag = BACKUP_NULL;
    g_pAddr = NULL;
    g_iLength = 0;
    
    return 0;
}

int Send2SyncThread(int iType, const void *pAddr, int iLength)
{
    //TO DO: may use message queue to send from thread

    return 0;
}

int batch_backup(const void *pAddr, int iLength)
{
    if(iLength > 0)
    {
        SetBackupFlag(BACKUP_BATCH, pAddr, iLength);
    }
    
    return 0;
}

int realtime_backup(const void *pAddr, int iLength, bool bIsInstant)
{
    if(iLength > 0)
    {
        if(bIsInstant == false)
        {
            SetBackupFlag(BACKUP_REALTIME_WAITING, pAddr, iLength);
        }
        else
        {
            SetBackupFlag(BACKUP_REALTIME_INSTANT, pAddr, iLength);
        }
    }

    return 0;
}
