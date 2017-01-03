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
#include "list.h"
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
int g_iLoginTimerFd = 0;
int g_iSyncSockFd = 0;
int g_iMainEventFd = 0;
int g_iSyncEventFd = 0;

extern stList *g_pstInstantList;
extern stList *g_pstWaitedList;

int _add_to_epoll(struct epoll_event stEvent, int iSyncEpollFd, int iAddFd)
{
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iAddFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    return epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, iAddFd, &stEvent);
}

int _ListTraverseAndResend(stList *pstList)
{
    stNode *pNode = pstList->pFront;
    pthread_mutex_lock(&pstList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->iSendTimers >= 3)//重发3次仍失败则删去节点
        {
            stNode *pNextNode = pNode->pNext;
            list_deleteByNode(g_pstInstantList, pNode);
            pNode = pNextNode;
            continue;
        }

        if(pNode->iFindTimers >= 3)//查找次数超过3次则重发
        {
            log_debug("pNode->iDataID(%d), pNode->iFindTimers(%d).", pNode->iDataID, pNode->iFindTimers);
            
            //resend
            MSG_NEWCFG_INSTANT_REQ *req = alloc_master_newCfgInstantReq(pNode->pData, pNode->iDataLen, pNode->iDataID);
            if(req == NULL)
            {
                log_error("alloc_master_newCfgInstantReq error!");
                return -1;
            }

            if(send2SlaveSync(g_iSyncSockFd, req, sizeof(MSG_NEWCFG_INSTANT_REQ) + pNode->iDataLen) < 0)
            {
                log_debug("Send to SLAVE SYNC failed!");
            }

            pNode->iFindTimers = 0;//查找次数清0
            pNode->iSendTimers++;//发送次数累加
        }

        pNode = pNode->pNext;
    }
    pthread_mutex_unlock(&pstList->pMutex);

    return 0;
}

void *master_sync(void *arg)
{
    log_info("SYNC task Beginning.");

    /*
     parse sync struct from main thread 
     */
    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;
    g_iMainEventFd = pstSyncStruct->iMainEventFd;
    g_iSyncEventFd = pstSyncStruct->iSyncEventFd;
    log_debug("g_iMainEventFd(%d), g_iSyncEventFd(%d)", g_iMainEventFd, g_iSyncEventFd);

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
    int iRet = _add_to_epoll(stEvent, iSyncEpollFd, g_iSyncEventFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iSyncEventFd error(%d)!", iRet);
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

    iRet = timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE);//3min
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

    iRet = timer_start(iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);//10min
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
     add iListTraverseTimerFd timerfd to epoll 
     */
    int iListTraverseTimerFd = timer_create();
    if(iListTraverseTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", iListTraverseTimerFd);
        return (void *)-1;
    }

    iRet = timer_start(iListTraverseTimerFd, LIST_TRAVERSE_TIMER_VALUE);//5min
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return (void *)-1;
    }

    iRet = _add_to_epoll(stEvent, iSyncEpollFd, iListTraverseTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add iListTraverseTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    /*
     add iNewcfgWaitedTimerFd to epoll 
     */
    int iNewcfgWaitedTimerFd = timer_create();
    if(iNewcfgWaitedTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", iNewcfgWaitedTimerFd);
        return (void *)-1;
    }

    iRet = _add_to_epoll(stEvent, iSyncEpollFd, iNewcfgWaitedTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add iNewcfgWaitedTimerFd error(%d)!", iRet);
        return (void *)-1;
    }

    /* memset buffer and struct */
    int iBufferSize;
    char acBuffer[MAX_BUFFER_SIZE];
    memset(acBuffer, 0, MAX_BUFFER_SIZE);

    void *pMsg;
    int iMsgLen;
    int iNewcfgWaitedLenSum = sizeof(MSG_NEWCFG_WAITED_REQ);
    int iTargetMsgLen = 0;
    char cFirstNewcfgWaitedFlag = 1, cNewcfgWaitedOkFlag = 1;

    while(1)
    {
        /* epoll wail */
        int iEpollNum = epoll_wait(iSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == g_iSyncEventFd && stEvents[i].events & EPOLLIN)
            {
                log_info("Get eventfd.");

                /* get events from g_iSyncEventFd */
                uint64_t uiEventsFlag;
                iRet = event_getEventFlags(g_iSyncEventFd, &uiEventsFlag);
                if(iRet < 0)
                {
                    log_error("event_getEventFlags error(%d)!", iRet);
                    return (void *)-1;
                }

                if(uiEventsFlag & MASTER_EVENT_NEWCFG_INSTANT) 
                {
                    log_info("Get MASTER_EVENT_NEWCFG_INSTANT.");

                    if(list_getReadSize(g_pstInstantList) != 0 && g_cMasterSyncStatus == STATUS_NEWCFG)
                    {
                        /* handle g_pstInstantList, send to slave */
                        log_debug("list_getReadSize(%d).", list_getReadSize(g_pstInstantList));

                        MSG_NEWCFG_INSTANT_REQ *req = alloc_master_newCfgInstantReq(g_pstInstantList->pReadNow->pData, g_pstInstantList->pReadNow->iDataLen, g_pstInstantList->pReadNow->iDataID);
                        if(req == NULL)
                        {
                            log_error("alloc_master_newCfgInstantReq error!");
                            return (void *)-1;
                        }

                        if(send2SlaveSync(g_iSyncSockFd, req, sizeof(MSG_NEWCFG_INSTANT_REQ) + g_pstInstantList->pReadNow->iDataLen) < 0)
                        {
                            log_debug("Send to SLAVE SYNC failed!");
                        }

                        g_pstInstantList->pReadNow->iFindTimers = 0;//查找次数清0
                        g_pstInstantList->pReadNow->iSendTimers++;//发送次数累加

                        /* list_read g_pstInstantList */
                        list_read(g_pstInstantList);
                        log_debug("list_getReadSize(%d).", list_getReadSize(g_pstInstantList));
                    }
                }

                if(uiEventsFlag & MASTER_EVENT_NEWCFG_WAITED) 
                {
                    log_info("Get MASTER_EVENT_NEWCFG_WAITED.");

                    //第一次收到MASTER_EVENT_NEWCFG_WAITED，开启定时阈值检测
                    if(cFirstNewcfgWaitedFlag)
                    {
                        iRet = timer_start(iNewcfgWaitedTimerFd, NEWCFG_WAITED_TIMER_VALUE);
                        if(iRet < 0)
                        {
                            log_error("sync timer start error(%d)!", iRet);
                            return (void *)-1;
                        }

                        cFirstNewcfgWaitedFlag = 0;
                    }

                    //检查定量
                    iNewcfgWaitedLenSum = iNewcfgWaitedLenSum + g_pstWaitedList->pRear->iDataLen + sizeof(DATA_NEWCFG);
                    if(iNewcfgWaitedLenSum >= MAX_PKG_LEN)//满足定量阈值，发送队列g_pstWaitedList中的配置
                    {
                        //第一次满足阈值，记录下当前不超过阈值的最大值
                        if(cNewcfgWaitedOkFlag)
                        {
                            iTargetMsgLen = iNewcfgWaitedLenSum - g_pstWaitedList->pRear->iDataLen - sizeof(DATA_NEWCFG);
                            cNewcfgWaitedOkFlag = 0;
                        }
                        
                        if(g_cMasterSyncStatus == STATUS_NEWCFG)
                        {
                            /* handle g_pstWaitedList, send to slave */
                            log_debug("list_getReadSize(%d).", list_getReadSize(g_pstWaitedList));

                            MSG_NEWCFG_WAITED_REQ *req = alloc_master_newCfgWaitedReq(iTargetMsgLen);
                            if(req == NULL)
                            {
                                log_error("alloc_master_newCfgWaitedReq error!");
                                return (void *)-1;
                            }
                            DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);

                            int iMsgLenSum = 0;
                            while(list_getReadSize(g_pstWaitedList) != 0 && iMsgLenSum < iTargetMsgLen)
                            {
                                iMsgLenSum = iMsgLenSum + g_pstWaitedList->pReadNow->iDataLen + sizeof(DATA_NEWCFG);

                                pDataNewcfg->iNewCfgID = htonl(g_pstWaitedList->pReadNow->iDataID);
                                pDataNewcfg->sChecksum = htons(checksum((const char *)g_pstWaitedList->pReadNow->pData, g_pstWaitedList->pReadNow->iDataLen));
                                pDataNewcfg->iDataLen = htonl(g_pstWaitedList->pReadNow->iDataLen);
                                memcpy(pDataNewcfg->acData, g_pstWaitedList->pReadNow->pData, g_pstWaitedList->pReadNow->iDataLen);

                                list_read(g_pstWaitedList);//移动队列的pReadNow指针向后一格
                                pDataNewcfg = (DATA_NEWCFG *)((char *)pDataNewcfg + g_pstWaitedList->pReadNow->iDataLen + sizeof(DATA_NEWCFG));//偏移指针以填充下一个配置包
                            }

                            req->sAllChecksum = htons(checksum((const char *)req->dataNewcfg, iTargetMsgLen - sizeof(MSG_NEWCFG_WAITED_REQ)));

                            if(send2SlaveSync(g_iSyncSockFd, pMsg, iTargetMsgLen) < 0)
                            {
                                log_debug("Send to SLAVE SYNC failed!");
                            }

                            log_debug("list_getReadSize(%d).", list_getReadSize(g_pstInstantList));
                            iNewcfgWaitedLenSum -= iTargetMsgLen;//重置队列中的配置长度之和 
                            cNewcfgWaitedOkFlag = 1;//重置第一次满足阈值的标志
                            iRet = timer_read(iNewcfgWaitedTimerFd);//重置定时阈值的定时器
                            if(iRet < 0)
                            {
                                log_error("sync timer_read error(%d)!", iRet);
                                return (void *)-1;
                            }
                        }
                    }
                }
            }//if g_iSyncEventFd
            else if(stEvents[i].data.fd == g_iLoginTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iLoginTimerFd.");
                if(g_cMasterSyncStatus == STATUS_LOGIN)
                {
                    //send login msg
                    MSG_LOGIN_RSP *rsp = (MSG_LOGIN_RSP *)alloc_master_rspMsg(CMD_LOGIN, g_cLoginRspSeq);
                    if(rsp == NULL)
                    {
                        log_error("alloc_master_rspMsg error!");
                        return (void *)-1;
                    }
                    rsp->cSynAckFlag = 1;//the second one in three-way handshake
                    rsp->cSpecifyID = g_cMasterSpecifyID;
                    if(send2SlaveSync(g_iSyncSockFd, rsp, sizeof(MSG_LOGIN_RSP)) < 0)
                    {
                        log_debug("Send to SLAVE SYNC failed!");
                    }
                }

                iRet = timer_read(g_iLoginTimerFd); //8s
                if(iRet < 0)
                {
                    log_error("login timer_read error(%d)!", iRet);
                    return (void *)-1;
                }
            }//else if g_iLoginTimerFd
            else if(stEvents[i].data.fd == g_iKeepaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iKeepaliveTimerFd.");

                if(g_cMasterSyncStatus == STATUS_NEWCFG)
                {
                    //no msg sent for a while, send keep alive msg
                    MSG_KEEP_ALIVE_REQ *req = (MSG_KEEP_ALIVE_REQ *)alloc_master_reqMsg(CMD_KEEP_ALIVE);
                    if(req == NULL)
                    {
                        log_error("alloc_master_reqMsg error!");
                        return (void *)-1;
                    }
                    req->cSpecifyID = g_cMasterSpecifyID;

                    if(send2SlaveSync(g_iSyncSockFd, req, sizeof(MSG_KEEP_ALIVE_REQ)) < 0)
                    {
                        log_debug("Send keepalive req to SLAVE SYNC failed!");
                    }
                }

                iRet = timer_read(g_iKeepaliveTimerFd); //3min
                if(iRet < 0)
                {
                    log_error("keep alive timer_read error(%d)!", iRet);
                    return (void *)-1;
                }

            }//else if g_iKeepaliveTimerFd
            else if(stEvents[i].data.fd == iCheckaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iCheckaliveTimerFd.");

                if(g_cMasterSyncStatus == STATUS_NEWCFG)
                {
                    //no msg recived for a while, relogin
                    srand((int)time(0));
                    g_cMasterSpecifyID = (char)(rand() % 0x100);
                    g_cMasterSyncStatus = STATUS_INIT;//change status into STATUS_INIT
                    //send event to main to restart
                    iRet = event_setEventFlags(g_iMainEventFd, MASTER_EVENT_CHECKALIVE_TIMER);
                    if(iRet < 0)
                    {
                        log_error("set g_iMainEventFd MASTER_EVENT_CHECKALIVE_TIMER error(%d)!", iRet);
                        return (void *)-1;
                    }
                }

                iRet = timer_read(iCheckaliveTimerFd);
                if(iRet < 0)
                {
                    log_error("check alive timer_read error(%d)!", iRet);
                    return (void *)-1;
                }

            }//else if iCheckaliveTimerFd
            else if(stEvents[i].data.fd == iListTraverseTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iListTraverseTimerFd.");

                _ListTraverseAndResend(g_pstInstantList);
                _ListTraverseAndResend(g_pstWaitedList);

                iRet = timer_read(iListTraverseTimerFd);//5min
                if(iRet < 0)
                {
                    log_error("sync timer_read error(%d)!", iRet);
                    return (void *)-1;
                }

            }//else if iListTraverseTimerFd
            else if(stEvents[i].data.fd == iNewcfgWaitedTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iNewcfgWaitedTimerFd.");

                //将当前队列g_pstWaitedList中的配置（肯定没有超过定量阈值）发送出去
                if(g_cMasterSyncStatus == STATUS_NEWCFG)
                {
                    /* handle g_pstWaitedList, send to slave */
                    log_debug("list_getReadSize(%d).", list_getReadSize(g_pstWaitedList));

                    MSG_NEWCFG_WAITED_REQ *req = alloc_master_newCfgWaitedReq(iNewcfgWaitedLenSum);
                    if(req == NULL)
                    {
                        log_error("alloc_master_newCfgWaitedReq error!");
                        return (void *)-1;
                    }
                    DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);

                    int iMsgLenSum = 0;
                    while(list_getReadSize(g_pstWaitedList) != 0 && iMsgLenSum < iNewcfgWaitedLenSum)
                    {
                        iMsgLenSum = iMsgLenSum + g_pstWaitedList->pReadNow->iDataLen + sizeof(DATA_NEWCFG);

                        pDataNewcfg->iNewCfgID = htonl(g_pstWaitedList->pReadNow->iDataID);
                        pDataNewcfg->sChecksum = htons(checksum((const char *)g_pstWaitedList->pReadNow->pData, g_pstWaitedList->pReadNow->iDataLen));
                        pDataNewcfg->iDataLen = htonl(g_pstWaitedList->pReadNow->iDataLen);
                        memcpy(pDataNewcfg->acData, g_pstWaitedList->pReadNow->pData, g_pstWaitedList->pReadNow->iDataLen);

                        list_read(g_pstWaitedList);//移动队列的pReadNow指针向后一格
                        pDataNewcfg = (DATA_NEWCFG *)((char *)pDataNewcfg + g_pstWaitedList->pReadNow->iDataLen + sizeof(DATA_NEWCFG));//偏移指针以填充下一个配置包
                    }

                    req->sAllChecksum = htons(checksum((const char *)req->dataNewcfg, iNewcfgWaitedLenSum - sizeof(MSG_NEWCFG_WAITED_REQ)));

                    if(send2SlaveSync(g_iSyncSockFd, pMsg, iNewcfgWaitedLenSum) < 0)
                    {
                        log_debug("Send to SLAVE SYNC failed!");
                    }

                    log_debug("list_getReadSize(%d).", list_getReadSize(g_pstInstantList));
                    iNewcfgWaitedLenSum = 0;//重置队列中的配置长度之和 
                }

                cFirstNewcfgWaitedFlag = 1;//下次NewcfgWaited到来时，再开启定时阈值检测
                iRet = timer_read(iNewcfgWaitedTimerFd);
                if(iRet < 0)
                {
                    log_error("sync timer_read error(%d)!", iRet);
                    return (void *)-1;
                }

            }//else if iNewcfgWaitedTimerFd
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



