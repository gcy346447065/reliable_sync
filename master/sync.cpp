#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include <stdlib.h> //for malloc rand
#include <time.h> //for time
#include <stdio.h>
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
#include "tool.h"
#include "instantList.h"
#include "waitedList.h"

char g_cMasterSyncStatus = STATUS_INIT;
char g_cLoginRspSeq = 0;
char g_cMasterSpecifyID = 0;
char g_cSlaveSpecifyID = 0;

int g_iSyncEpollFd = 0;
int g_iSyncSockFd = 0;
int g_iLoginSynAckTimerFd = 0;
int g_iKeepaliveTimerFd = 0;
int g_iCheckaliveTimerFd = 0;
int g_iInstantTimerFd = 0;
int g_iWaitedTimerFd = 0;

extern int g_iMainEventFd;
extern int g_iSyncEventFd;

int master_sync_init(void)
{
    g_iSyncEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(g_iSyncEpollFd < 0)
    {
        log_error("epoll create error(%d)!", g_iSyncEpollFd);
        return -1;
    }

    //将g_iSyncEventFd添加到epoll中，由于main线程会向sync发送事件，所以需要由main创建
    int iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iSyncEventFd);
    if(iRet < 0)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", g_iSyncEpollFd, g_iSyncEventFd, iRet);
        return -1;
    }

    //将g_iSyncSockFd添加到epoll中
    g_iSyncSockFd = socket_init();//creat, bind, connect
    if(g_iSyncSockFd < 0)
    {
        log_error("socket init error(%d)!", g_iSyncSockFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iSyncSockFd);
    if(iRet < 0)
    {
        log_error("g_iSyncEpollFd add g_iSyncSockFd error(%d)!", iRet);
        return -1;
    }

    //收到slave发来的syn登录包后开启此定时器，用于循环发送SynAck登录包
    g_iLoginSynAckTimerFd = timer_create();
    if(g_iLoginSynAckTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iLoginSynAckTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iLoginSynAckTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iLoginSynAckTimerFd error(%d)!", iRet);
        return -1;
    }
    srand((int)time(0));
    g_cMasterSpecifyID = (char)(rand() % 0x100);//生成设备识别ID

    //发送一次消息则重启此定时器，用于一段时间无消息发送时做连接保活
    g_iKeepaliveTimerFd = timer_create();
    if(g_iKeepaliveTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iKeepaliveTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iKeepaliveTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iKeepaliveTimerFd error(%d)!", iRet);
        return -1;
    }

    //接收一次消息则重启此定时器，用于一段时间未收到消息说明对端故障
    g_iCheckaliveTimerFd = timer_create();
    if(g_iCheckaliveTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iCheckaliveTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iCheckaliveTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iCheckaliveTimerFd error(%d)!", iRet);
        return -1;
    }

    //遍历一次instant链表则重启此定时器，如发现链表为空则关闭定时器直至再次收到instant事件开启定时器
    //用于instant链表配置的重发与多次发送仍失败的删除
    g_iInstantTimerFd = timer_create();
    if(g_iInstantTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iInstantTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iInstantTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iInstantTimerFd error(%d)!", iRet);
        return -1;
    }

    //第一次收到waited事件时开启定时器，如定时定量发送过一次则重启定时器，如发现节点发送次数达上限则删除，
    //如发现链表为空则关闭定时器直至再次收到waited事件开启定时器，用于waited链表的定时发送
    g_iWaitedTimerFd = timer_create();
    if(g_iWaitedTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iWaitedTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iWaitedTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iWaitedTimerFd error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_syncSocket(void)
{
    log_info("Get g_iSyncSockFd.");

    int iRet = 0;
    int iBufferSize = 0;
    char *pcBuffer = (char *)malloc(MAX_BUFFER_SIZE);
    memset(pcBuffer, 0, MAX_BUFFER_SIZE);
    if((iBufferSize = recvFromSlaveSync(g_iSyncSockFd, pcBuffer, MAX_BUFFER_SIZE)) > 0)
    {
        log_hex(pcBuffer, iBufferSize);

        iRet = handle_sync_msg(pcBuffer, iBufferSize);
        if(iRet < 0)
        {
            log_error("handle_sync_msg error!");
        }
    }

    free(pcBuffer);
    return 0;
}

int _epoll_syncEvent(void)
{
    log_info("Get g_iSyncEventFd.");

    //获取事件标志，共有64种事件，可同时触发多个
    uint64_t uiEventsFlag;
    int iRet = event_getEventFlags(g_iSyncEventFd, &uiEventsFlag);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }

    //触发instant事件，向对端sync模块发送配置消息
    if(uiEventsFlag & MASTER_SYNC_EVENT_NEWCFG_INSTANT)
    {
        log_info("Get MASTER_SYNC_EVENT_NEWCFG_INSTANT.");
        log_debug("hehe.");
        log_debug("%u", instantList_getNewSize());
        printf("listSize:%u\n", instantList_getNewSize());
        if(g_cMasterSyncStatus == STATUS_NEWCFG && instantList_getNewSize() > 0)
        {
            stInstantNode *pstNewNode= instantList_getNewNode();
            printf("instantList_getNewNode:%d\n", pstNewNode);
            MSG_NEWCFG_INSTANT_REQ *req = alloc_master_newCfgInstantReq(pstNewNode->pData, pstNewNode->iDataLen, pstNewNode->uiInstantID);
            if(req == NULL)
            {
                log_error("alloc_master_newCfgInstantReq error!");
                return -1;
            }

            if(sendToSlaveSync(g_iSyncSockFd, req, sizeof(MSG_NEWCFG_INSTANT_REQ) + pstNewNode->iDataLen) < 0)
            {
                log_debug("Send to SLAVE SYNC failed!");
            }

            pstNewNode->cFindTimers = 0;//查找次数清0
            pstNewNode->cSendTimers++;//发送次数累加
            printf("moveNew:%d\n", instantList_moveNew());
        }
        log_debug("hehe.");
    }

    //触发waited事件，向对端sync模块发送配置消息
    if(uiEventsFlag & MASTER_SYNC_EVENT_NEWCFG_WAITED)
    {printf("get uiEventsFlag\n");
        log_info("Get MASTER_SYNC_EVENT_NEWCFG_WAITED.");printf("g_cMasterSyncStatus:%d waitedList_getMsgLen:%d\n", g_cMasterSyncStatus, waitedList_getMsgLen());

        if(g_cMasterSyncStatus == STATUS_NEWCFG && waitedList_getMsgLen() >= MAX_PKG_LEN)
        {printf("len is OK\n");
            MSG_NEWCFG_WAITED_REQ *req = alloc_master_newCfgWaitedReq(waitedList_getMsgLen());
            if(req == NULL)
            {
                log_error("alloc_master_newCfgWaitedReq(%d) error!", waitedList_getMsgLen());
                return -1;
            }printf("req is OK\n");

            waitedList_traverseAndPack(req);printf("have packed\n");

            //可能在打包过程中删除了节点，导致g_pstWaitedList->uiMsgLen变小
            if(sendToSlaveSync(g_iSyncSockFd, req, waitedList_getMsgLen()) < 0)
            {
                log_debug("Send to SLAVE SYNC failed!");
            }printf("send done\n");

            iRet = timer_start(g_iWaitedTimerFd, NEWCFG_WAITED_TIMER_VALUE);
            if(iRet < 0)
            {
                log_error("sync timer start error(%d)!", iRet);
                return -1;
            }
        }
    }

    return 0;
}

int _epoll_loginSynAckTimer(void)
{
    log_info("Get g_iLoginSynAckTimerFd.");

    if(g_cMasterSyncStatus == STATUS_LOGIN)
    {
        //send login msg
        MSG_LOGIN_RSP *rsp = (MSG_LOGIN_RSP *)alloc_master_rspMsg(CMD_LOGIN, g_cLoginRspSeq);
        if(rsp == NULL)
        {
            log_error("alloc_master_rspMsg error!");
            return -1;
        }
        rsp->cSynAckFlag = 1;//the second one in three-way handshake
        rsp->cSpecifyID = g_cMasterSpecifyID;
        if(sendToSlaveSync(g_iSyncSockFd, rsp, sizeof(MSG_LOGIN_RSP)) < 0)
        {
            log_debug("Send to SLAVE SYNC failed!");
        }
    }

    int iRet = timer_start(g_iLoginSynAckTimerFd, LOGIN_TIMER_VALUE); //8s
    if(iRet < 0)
    {
        log_error("login timer_start error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_keepaliveTimer(void)
{
    log_info("Get g_iKeepaliveTimerFd.");

    if(g_cMasterSyncStatus == STATUS_NEWCFG)
    {
        //no msg sent for a while, send keep alive msg
        MSG_KEEP_ALIVE_REQ *req = (MSG_KEEP_ALIVE_REQ *)alloc_master_reqMsg(CMD_KEEP_ALIVE);
        if(req == NULL)
        {
            log_error("alloc_master_reqMsg error!");
            return -1;
        }
        req->cSpecifyID = g_cMasterSpecifyID;

        if(sendToSlaveSync(g_iSyncSockFd, req, sizeof(MSG_KEEP_ALIVE_REQ)) < 0)
        {
            log_debug("Send keepalive req to SLAVE SYNC failed!");
        }
    }

    int iRet = timer_start(g_iKeepaliveTimerFd, KEEPALIVE_TIMER_VALUE); //3min
    if(iRet < 0)
    {
        log_error("keep alive timer_start error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_checkaliveTimer(void)
{
    log_info("Get g_iCheckaliveTimerFd.");

    int iRet = 0;
    if(g_cMasterSyncStatus == STATUS_NEWCFG)
    {
        //no msg recived for a while, send event to main to restart
        iRet = event_setEventFlags(g_iMainEventFd, MASTER_EVENT_CHECKALIVE_TIMER);
        if(iRet < 0)
        {
            log_error("set g_iMainEventFd MASTER_EVENT_CHECKALIVE_TIMER error(%d)!", iRet);
            return -1;
        }
    }

    iRet = timer_start(g_iCheckaliveTimerFd, CHECKALIVE_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("check alive timer_start error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_instantTimer(void)
{
    log_info("Get g_iInstantTimerFd.");

    if(g_cMasterSyncStatus == STATUS_NEWCFG)
    {
        instantList_traverseAndResend();
    }

    int iRet = timer_start(g_iInstantTimerFd, NEWCFG_INSTANT_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("instant timer_start error(%d)!", iRet);
        return -1;
    }

    return 0;
}

int _epoll_waitedTimer(void)
{
    log_info("Get g_iWaitedTimerFd.");

    if(g_cMasterSyncStatus == STATUS_NEWCFG && waitedList_getMsgLen() > 0)
    {
        MSG_NEWCFG_WAITED_REQ *req = alloc_master_newCfgWaitedReq(waitedList_getMsgLen());
        if(req == NULL)
        {
            log_error("alloc_master_newCfgWaitedReq error!");
            return -1;
        }

        waitedList_traverseAndPack(req);

        //可能在打包过程中删除了节点，导致g_pstWaitedList->uiMsgLen变小
        if(sendToSlaveSync(g_iSyncSockFd, req, waitedList_getMsgLen()) < 0)
        {
            log_debug("Send to SLAVE SYNC failed!");
        }
    }

    int iRet = timer_start(g_iWaitedTimerFd, NEWCFG_WAITED_TIMER_VALUE);
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return -1;
    }

    return 0;
}

/*
 * sync模块的起点，由main线程创建
 */
void *master_sync_thread(void *arg)
{
    log_info("SYNC Task Beginning.");

    struct sync_struct *pstSyncStruct = (struct sync_struct *)arg;

    int iRet = master_sync_init();
    if(iRet < 0)
    {
        log_error("master_sync_init error!");
        return (void *)-1;
    }

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(1)
    {
        int iEpollNum = epoll_wait(g_iSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == g_iSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_syncSocket();
                if(iRet < 0)
                {
                    log_warning("_epoll_syncSocket failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iSyncEventFd && stEvents[i].events & EPOLLIN)
            {
                //控制台有输入，用于测试时触发下配置
                iRet = _epoll_syncEvent();
                if(iRet < 0)
                {
                    log_warning("_epoll_syncEvent failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iLoginSynAckTimerFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_loginSynAckTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_loginSynAckTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iKeepaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_keepaliveTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_keepaliveTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iCheckaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_checkaliveTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_checkaliveTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iInstantTimerFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_instantTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_instantTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iWaitedTimerFd && stEvents[i].events & EPOLLIN)
            {
                //
                iRet = _epoll_waitedTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_waitedTimer failed!");
                }
            }
        }
    }

    log_info("SYNC Task Ending.");
    return (void *)0;
}

