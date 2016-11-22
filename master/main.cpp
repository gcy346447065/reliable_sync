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


int SendToSync(int iMainToSyncSockFd, void *pBuf, int iBufLen, int iMaxPkgLen, void *pDestAddr, int iSendMethod)
{
    if(pBuf == NULL || iBufLen == 0)
    {
        log_error("SendToSync pBuf or iBufLen error!");
        return -1;
    }
    //TO DO: iMaxPkgLen, pDestAddr unused

    switch(iSendMethod)
    {
        case SEND_NEWCFG_WAITED:
            log_info("SEND_REALTIME_WAITED.");
            break;

        case SEND_NEWCFG_INSTANT:
            log_info("SEND_REALTIME_INSTANT.");

            if(write(iMainToSyncSockFd, "hehe", 4) < 0)
            {
                log_error("Send hehe to server error!");
            }

            break;
            
        default:
            log_info("iSendMethod:%d.", iSendMethod);
            break;
    }

    log_info("SendToSync ok.");
    return 0;
}

int RecvFromSync(void *pBuf, int iBufLen)
{
    return 0;
}

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

    /* add main eventfd to epoll */
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

    /* set iMainToSyncSockFd bind and connect, add iMainToSyncSockFd to epoll */
    int iMainToSyncSockFd = socket_init();
    if(iMainToSyncSockFd < 0)
    {
        log_error("socket init error(%d)!", iMainToSyncSockFd);
        return -1;
    }
    log_debug("iMainToSyncSockFd(%d).", iMainToSyncSockFd);

    struct sockaddr_in stMasterMainAddr;
    memset(&stMasterMainAddr, 0, sizeof(stMasterMainAddr)); 
    stMasterMainAddr.sin_family = AF_INET; 
    stMasterMainAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterMainAddr.sin_port = htons(MASTER_MAIN_PORT);
    if(bind(iMainToSyncSockFd, (struct sockaddr*)&stMasterMainAddr, sizeof(stMasterMainAddr)) < 0)
    {
        log_error("socket init error(%d)!", stMasterMainAddr);
        return -1;
    }

    struct sockaddr_in stMasterSyncAddr;
    memset(&stMasterSyncAddr, 0, sizeof(stMasterSyncAddr)); 
    stMasterSyncAddr.sin_family = AF_INET; 
    stMasterSyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSyncAddr.sin_port = htons(MASTER_SYNC_PORT);
    if(connect(iMainToSyncSockFd, (struct sockaddr*)&stMasterSyncAddr, sizeof(stMasterSyncAddr)) < 0)
    {
        log_error("socket init error(%d)!", iMainToSyncSockFd);
        return -1;
    }

    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iMainToSyncSockFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iMainEpollFd, EPOLL_CTL_ADD, iMainToSyncSockFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add iMainToSyncSockFd error(%d)!", iRet);
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
    iRet = pthread_create(&SyncThreadId, NULL, master_sync, (void *)&stSyncStruct);
    if(iRet != 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    /* log for Beginning */
    log_info("Main Task Beginning.");

    char *pcRealtimeBuf = NULL;
    int iRealtimeBufLen = 0;
    char *pcBatchBuf; //TO DO
    int iBatchBufLen;
    int iNewCfgFd; //opened when reading STDIN, realtime SendToSync when get new config event
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
                char acStdinBuf[128], acCutBuf[128];
                memset(acStdinBuf, 0, 128);
                memset(acCutBuf, 0, 128);

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

                    iNewCfgFd = open(pcRestBuf, O_RDONLY);
                    if(iNewCfgFd > 0)
                    {
                        log_info("open new config file ok.");
                        iRet = event_setEventFlags(iMainEventFd, EVENT_FLAG_MASTER_NEWCFG);
                        if(iRet == 0)
                        {
                            log_info("event_setEventFlags EVENT_FLAG_MASTER_NEWCFG ok.");
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
                log_info("Get eventfd.");

                /* get events from iMainEventFd */
                uint64_t uiEventsFlag;
                iRet = event_getEventFlags(iMainEventFd, &uiEventsFlag);
                if(iRet < 0)
                {
                    log_error("event_getEventFlags error(%d)!", iRet);
                    return -1;
                }

                if(uiEventsFlag & EVENT_FLAG_MASTER_NEWCFG) //planned: set by main mask, tested: when get STDIN_FILENO keys in
                {
                    log_info("Get master new config event flag.");

                    /* realtime SendToSync */
                    char *pcNewCfgBuf = (char *)malloc(1024);
                    memset(pcNewCfgBuf, 0, 1024);
                    iRet = read(iNewCfgFd, pcNewCfgBuf, 1024);
                    if(iRet < 0)
                    {
                        log_error("read iNewCfgFd error(%d)!", iRet);
                        return -1;
                    }
                    int iBufLen = iRet;

                    iRet = SendToSync(iMainToSyncSockFd, pcNewCfgBuf, iBufLen, 1024, NULL, SEND_NEWCFG_INSTANT);
                    if(iRet < 0)
                    {
                        log_error("SendToSync error(%d)!", iRet);
                    }
                }

                if(uiEventsFlag & EVENT_FLAG_SLAVE_RESTART) //set the flag when sync thread find no keep alive ack
                {
                    log_info("Get slave restart event flag.");

                    /* batch SendToSync */
                }

                if(uiEventsFlag & EVENT_FLAG_SYNC_TIMER) //set the flag when sync thread find no keep alive ack
                {
                    log_info("Get sync batch timer event flag.");

                    /* batch SendToSync */
                }
            }//else if iMainEventFd
            else if(stEvents[i].data.fd == iMainToSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                //
            }//else if iMainToSyncSockFd
            
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
