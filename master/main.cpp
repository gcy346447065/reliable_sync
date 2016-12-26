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
    SEND_SET_SYNC_TIMER,
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
            break;

        case SEND_NEWCFG_INSTANT:
            log_info("SEND_NEWCFG_INSTANT.");
            queue_push(pstQueue, pBuf, iBufLen);
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

    int iNewCfgFd; //opened when reading STDIN, realtime SendToSync when get new config event
    int iFileBegin, iFileEnd;
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

                if(char *pcFilename = strstr(acStdinBuf, NEW_CFG_INSTANT_START_STR)) //"?"
                {
                    pcFilename += NEW_CFG_INSTANT_START_STR_LEN; //strlen("?")
                    log_info("Get new config, filename: \"%s\".", pcFilename);

                    iNewCfgFd = open(pcFilename, O_RDONLY);
                    if(iNewCfgFd > 0)
                    {
                        log_info("open INSTANT file ok.");
                        iRet = event_setEventFlags(iMainEventFd, MASTER_EVENT_KEYIN_INSTANT);
                        if(iRet == 0)
                        {
                            log_info("set event flag MASTER_EVENT_KEYIN_INSTANT ok.");
                        }
                    }
                    else
                    {
                        log_error("open new config file error(%s)!", strerror(errno));
                    }
                }
                else if(sscanf(acStdinBuf, "/file%d:file%d", iFileBegin, iFileEnd)) //"/file8:file10"
                {
                    char *pcFilenameBegin, *pcFilenameEnd;
                    sprintf(pcFilenameBegin, "file%d", iFileBegin);
                    sprintf(pcFilenameEnd, "file%d", iFileEnd);
                    if(open(pcFilenameBegin, O_RDONLY) > 0 && open(pcFilenameEnd, O_RDONLY) > 0)
                    {
                        log_info("open WAITED files ok.");
                        iRet = event_setEventFlags(iMainEventFd, MASTER_EVENT_KEYIN_WAITED);
                        if(iRet == 0)
                        {
                            log_info("set event flag MASTER_EVENT_KEYIN_WAITED ok.");
                        }
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
                    char *pcNewCfgBuf = (char *)malloc(MAX_NEW_CFG_LEN);
                    memset(pcNewCfgBuf, 0, MAX_NEW_CFG_LEN);
                    iRet = read(iNewCfgFd, pcNewCfgBuf, MAX_NEW_CFG_LEN);
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
