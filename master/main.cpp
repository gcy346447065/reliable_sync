
#include <pthread.h>
#include <netinet/in.h>

#include <signal.h>



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

bool g_bSendSynAckFlag = false;
int g_iSocketStatus = SERVER_SOCKET_WAIT_SYN;
int g_iBackupStatus = BACKUP_NULL;

void *master_service(void *arg);

int getNewConfig()
{
    return 1;
}

int getSlaveRestart()
{
    return 1;
}

int getSyncTimer()
{
    return 1;
}

int main(int argc, char *argv[])
{
    /* log init */
    log_init();

    /* pthread create */
    pthread_t SyncThreadId;
    int iRet = pthread_create(&SyncThreadId, NULL, master_service, NULL);
    if(iRet != 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    /* epoll create */
    int iEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(iEpollFd < 0)
    {
        log_error("epoll create error(%d)!", iEpollFd);
        return -1;
    }
    struct epoll_event stEvent, stEvents[MAX_EPOLL_NUM];

    /* add sync timerfd to epoll */
    int iSyncTimerFd = timer_start(1000 * 60 * 1); //planned: 10 min, tested: 1 min
    if(iSyncTimerFd < 0)
    {
        log_error("sync timer start error(%d)!", iSyncTimerFd);
        return -1;
    }
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iSyncTimerFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, iSyncTimerFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iSyncTimerFd error(%d)!", iRet);
        return -1;
    }

    /* add eventfd to epoll */
    int iEventFd = event_init(0); //0 for event flag init value
    if(iEventFd < 0)
    {
        log_error("eventfd error(%d)!", iEventFd);
        return -1;
    }
    log_debug("iEventFd(%d)", iEventFd);
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, iEventFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iEventFd error(%d)!", iRet);
        return -1;
    }

    /* add STDIN_FILENO to epoll for test(to trigger master find new config) */
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = STDIN_FILENO;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, STDIN_FILENO, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add STDIN_FILENO error(%d)!", iRet);
        return -1;
    }

    /* log for Beginning */
    log_info("Main Task Beginning.");

    char *pcRealtimeBuf = NULL;
    int iRealtimeBufLen = 0;
    char *pcBatchBuf;//TO DO
    int iBatchBufLen;
    while(1)
    {
        /* main task */

        /* epoll wail */
        int iEpollNum = epoll_wait(iEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            /* realtime SendToSync when master find new config */
            /* batch SendToSync when find slave restart or get timer */
            if(stEvents[i].data.fd == iSyncTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_info("Get sync timer fd.");

                /* batch SendToSync */

                /* read timerfd to loop again */
                uint64_t uiSyncTimerRead;
                iRet = read(iSyncTimerFd, &uiSyncTimerRead, sizeof(uint64_t));
                if(iRet != sizeof(uint64_t))
                {
                    log_error("read iSyncTimerFd error(%s)!", strerror(errno));
                    return -1;
                }
            }
            else if(stEvents[i].data.fd == iEventFd && stEvents[i].events & EPOLLIN)
            {
                log_info("Get eventfd.");

                /* get events from iEventFd */
                uint64_t uiEventsFlag;
                iRet = event_getEventFlags(iEventFd, &uiEventsFlag);
                if(iRet < 0)
                {
                    log_error("event_getEventFlags error(%d)!", iRet);
                    return -1;
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
            }
            else if(stEvents[i].data.fd == STDIN_FILENO && stEvents[i].events & EPOLLIN)
            {
                log_info("Get STDIN_FILENO fd.");

                /* read STDIN_FILENO */
                char acStdinBuf[128], acCutBuf[128], acRealtimeBuf[1024];
                memset(acStdinBuf, 0, 128);
                memset(acCutBuf, 0, 128);
                memset(acRealtimeBuf, 0, 1024);

                iRet = read(STDIN_FILENO, acStdinBuf, 128);
                if(iRet < 0)
                {
                    log_error("read STDIN_FILENO error(%s)!", strerror(errno));
                    return -1;
                }
                memcpy(acCutBuf, acStdinBuf, iRet-1); //-1 for '\n'

                char *pcRestBuf = NULL;
                if(pcRestBuf = strstr(acCutBuf, "new config "))
                {
                    pcRestBuf += 11; //strlen("new config ")
                    log_info("Get new config, filename: \"%s\".", pcRestBuf);

                    int iOpenFd = open(pcRestBuf, O_RDONLY);
                    if(iOpenFd > 0)
                    {
                        log_info("open new config file ok.");

                        log_debug("iRet(%d), iEventFd(%d), ", iRet, iEventFd);
                        iRet = event_setEventFlags(iEventFd, EVENT_FLAG_MASTER_NEWCFG);
                        if(iRet == 0)
                        {
                            log_info("event_setEventFlags EVENT_FLAG_MASTER_NEWCFG ok.");
                        }
                        log_debug("iRet(%d)", iRet);
                    }
                    else
                    {
                        log_error("open new config file error(%s)!", strerror(errno));
                    }
                }
            }
        }


        #if 0
        /* realtime SendToSync when find new config */
        if(getNewConfig(&pRealtimeBuf, &iRealtimeBufLen))
        {
            SendToSync(pRealtimeBuf, iRealtimeBufLen, 0, NULL, SEND_REALTIME_WAITED);
        }

        /* batch SendToSync when find slave restart or regularly */
        if(getSlaveRestart() || getSyncTimer())
        {
            SendToSync(pBatchBuf, pBatchBuf, 0, NULL, SEND_BATCH);
        }
        #endif
    }

    /* free all */
    log_info("Main Task Ending.");
    log_free();
    close(iEpollFd);
    close(iSyncTimerFd);
    return 0;
}

void *master_service(void *arg)
{
    //TO DO
}

#if 0
void SetSendSynAckFlag(int iSig)
{
    if(iSig == SIGALRM)
    {
        log_debug("Set send SYN ACK flag again.");
        g_bSendSynAckFlag = true;
    }
}

void *server_service(void *arg)
{
    log_info("Server Service Beginning.");

    /* socket init */
    int iRet = socket_init();
    if(iRet < 0)
    {
        log_error("socket init error(%d)!", iRet);
        return (void *)-1;
    }
    int iSockFd = iRet;
    log_debug("Server iSockFd=%d.", iSockFd);
    
    /* memset buffer and struct */
    char acBuffer[BUFFER_SIZE];
    memset(acBuffer, 0, BUFFER_SIZE);

    char acReadBuffer[BUFFER_SIZE];
    memset(acReadBuffer, 0, BUFFER_SIZE);

    struct sockaddr_in stRecvAddr;
    memset(&stRecvAddr, 0, sizeof(stRecvAddr));
    int iRecvAddrLen = 0;

    /* set timer alarm signal callback for setting send SYN ACK flag */
    signal(SIGALRM, SetSendSynAckFlag);

    int iBackupType;
    const void *pAddr;
    int iLength;

    while(1)
    {
        //avoid 100% CPU
        sleep(1); 
        
        /* wait for socket msg */ //TO OD: epoll?
        memset(acBuffer, 0, BUFFER_SIZE);
        int iRecvLen = recvfrom(iSockFd, acBuffer, BUFFER_SIZE, 0, (struct sockaddr *)&stRecvAddr, (socklen_t *)&iRecvAddrLen);
        if(iRecvLen > 0)
        {
            //TO DO: the stRecvAddr of the first msg is NULL
            log_debug("Get socket msg: %d, %s.", stRecvAddr.sin_port, acBuffer);
        }

        /* adjust and remember the socket status */ //TO DO: timeout to reset status
        switch(g_iSocketStatus)
        {
            case SERVER_SOCKET_WAIT_SYN:
                if(strcmp(acBuffer, "SYN") == 0)
                {
                    g_iSocketStatus = SERVER_SOCKET_SEND_SYN_ACK;
                    g_bSendSynAckFlag = true;
                }
                break;

            case SERVER_SOCKET_SEND_SYN_ACK:
                if(strcmp(acBuffer, "ACK") != 0)
                {
                    if(g_bSendSynAckFlag)
                    {
                        if(sendto(iSockFd, "SYN ACK", 7, 0, (struct sockaddr *)&stRecvAddr, iRecvAddrLen) < 0)
                        {
                            log_debug("Send SYN ACK to client failed!");
                            g_bSendSynAckFlag = false;
                            alarm(3);//3s to set g_bSendSynAckFlag = true;
                        }
                        else
                        {
                            log_debug("Send SYN ACK ok.");
                            g_bSendSynAckFlag = false;
                        }
                    }
                }
                else
                {
                    log_debug("Recv ACK ok. SERVER READY.");
                    g_iSocketStatus = SERVER_SOCKET_READY;
                }
                break;

            case SERVER_SOCKET_READY:
                //TO DO: keep alive, reset status when failed
                
                /* wait for backup msg */ //TO OD: recv from message queue
                GetBackupFlag(&g_iBackupStatus, &pAddr, &iLength);
                if(g_iBackupStatus > BACKUP_NULL)
                {
                    /* get backup msg, remember the backup status */
                    log_debug("Get backup msg: %d, %s, %d.", g_iBackupStatus, pAddr, iLength);

                    //open backup file with backup msg
                    int iFileFd = open((const char *)pAddr, O_RDONLY);
                    if(iFileFd < 0)
                    {
                        log_error("Open backup file error!");
                        return (void *)-1;
                    }

                    //stat backup file to check file size
                    struct stat fileStat;
                    if(stat((const char *)pAddr, &fileStat) < 0)
                    {
                        log_error("Stat backup file error!");
                        return (void *)-2;
                    }
                    if(fileStat.st_size != iLength)
                    {
                        log_error("Backup file size error!");
                        return (void *)-3;
                    }

                    //send the backup file
                    log_debug("Send backup file now, iLength = %d.", iLength);
                    int iSeekOffset = 0;
                    do
                    {
                        int iSeekRet = lseek(iFileFd, iSeekOffset, SEEK_SET);
                        if(iSeekRet < 0)
                        {
                            log_error("Seek backup file error!");
                            return (void *)-4;
                        }

                        memset(acReadBuffer, 0, BUFFER_SIZE);
                        int iReadRet = read(iFileFd, acReadBuffer, BUFFER_SIZE);
                        if(iReadRet < 0)
                        {
                            log_error("Read backup file error!");
                            return (void *)-5;
                        }

                        iSeekOffset += iReadRet;

                        if(sendto(iSockFd, acReadBuffer, iReadRet, 0, (struct sockaddr *)&stRecvAddr, iRecvAddrLen) < 0)
                        {
                            log_error("Send backup file error!");
                            //TO DO: resend
                        }
                        else
                        {
                            log_debug("Send backup file ok, iSeekOffset = %d.", iSeekOffset);
                        }
                    }while(iSeekOffset < iLength);

                    ResetBackupFlag();
                }
                break;

            default:
                log_error("Unknown server status!");
                return (void *)-6;
        }
    }

    log_info("Server Service Ending.");

    socket_free(iSockFd);
    return (void *)NULL;
}
#endif
