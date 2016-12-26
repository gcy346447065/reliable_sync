#include <netinet/in.h> //for sockaddr_in
#include <arpa/inet.h> //for inet_addr
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include <errno.h> //for errno
#include <unistd.h> //for read
#include <stdlib.h> //for malloc rand
#include <time.h> //for time
#include "sync.h"
#include "macro.h"
#include "log.h"
#include "socket.h"
#include "event.h"
#include "timer.h"
#include "checksum.h"
#include "protocol.h"

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_SEND_CFG = 2
};

static char g_slaveSyncStatus = STATUS_INIT;
static char g_masterSpecifyNum = 0;
static char g_slaveSpecifyNum = 0;
static char g_seq = 0;

static int g_iLoginTimerFd = 0;

typedef int (*MSG_PROC)(int iSockFd, const char *pcMsg);
typedef struct
{
    char cmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

MSG_HEADER *alloc_slave_reqMsg(char cmd, int length)
{
    MSG_HEADER *msg = (MSG_HEADER *)malloc(length);

    if(msg)
    {
        msg->signature = htons(START_FLAG);
        msg->cmd = cmd;
        msg->seq = g_seq++;
        msg->length = htons(length - MSG_HEADER_LEN);
    }

    return msg;
}

MSG_HEADER *alloc_slave_rspMsg(const MSG_HEADER *pMsg)
{
    int iMsgLen = 0;
    switch(pMsg->cmd)
    {
        case CMD_LOGIN:
            iMsgLen = sizeof(MSG_LOGIN_RSP);
            break;

        case CMD_NEW_CFG:
            iMsgLen = sizeof(MSG_NEW_CFG_RSP);
            break;

        case CMD_KEEP_ALIVE:
            iMsgLen = sizeof(MSG_KEEP_ALIVE_RSP);
            break;

        default:
            return NULL;
    }

    MSG_HEADER *pMsgHeader = (MSG_HEADER *)malloc(iMsgLen);
    if(pMsgHeader)
    {
        pMsgHeader->signature = htons(START_FLAG);
        pMsgHeader->cmd = pMsg->cmd;
        pMsgHeader->seq = pMsg->seq;
        pMsgHeader->length = htons(iMsgLen - MSG_HEADER_LEN);
    }

    return pMsgHeader;
}

static int sync_login(int iSockFd, const char *pcMsg)
{
    const MSG_LOGIN_RSP *rsp = (const MSG_LOGIN_RSP *)pcMsg;
    if(!rsp)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(rsp->header.length) < sizeof(MSG_LOGIN_RSP) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    if(rsp->synAckFlag == 1)
    {
        //get the second one in three-way handshake, send the third one as req
        MSG_LOGIN_REQ *req = (MSG_LOGIN_REQ *)alloc_slave_reqMsg(CMD_LOGIN, sizeof(MSG_LOGIN_REQ));
        if(req == NULL)
        {
            log_error("alloc_slave_reqMsg MSG_LOGIN_REQ error!");
            return -1;
        }
        req->synFlag = 0;//the third one in three-way handshake
        req->ackFlag = 1;
        req->specifyNum = g_slaveSpecifyNum;
        if(write(iSockFd, req, sizeof(MSG_LOGIN_REQ)) < 0) //after connect, write = send
        {
            log_debug("Send to SLAVE SYNC failed!");
        }

        timer_stop(g_iLoginTimerFd);
    }

    return 0;
}

static int sync_newCfg(int iSockFd, const char *pcMsg)
{
    const MSG_NEW_CFG_REQ *req = (const MSG_NEW_CFG_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg req empty!");
        return -1;
    }
    if(ntohs(req->header.length) < sizeof(MSG_NEW_CFG_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    log_info("req->newCfgNum(%d), req->checksum(%d).", ntohs(req->newCfgNum), ntohs(req->checksum));

    MSG_NEW_CFG_RSP *rsp = (MSG_NEW_CFG_RSP *)alloc_slave_rspMsg((const MSG_HEADER *)pcMsg);
    if(!rsp)
    {
        log_error("msg rsp empty!");
        return -1;
    }

    rsp->newCfgNum = req->newCfgNum;
    short iSlaveChecksum = checksum((const char *)(req->data), ntohs(req->header.length) - sizeof(short)*2);
    log_info("iSlaveChecksum(%d)", iSlaveChecksum);
    if(iSlaveChecksum == ntohs(req->checksum))
    {
        rsp->result = NEW_CFG_RESULT_SUCCEED;
    }
    else
    {
        rsp->result = NEW_CFG_RESULT_FAILED;
    }

    if(write(iSockFd, rsp, sizeof(MSG_NEW_CFG_RSP)) < 0) //after connect, write = send
    {
        log_debug("Send to MASTER MSG_NEW_CFG_RSP failed!");
        return -1;
    }

    return 0;
}

static int sync_keepAlive(int iSockFd, const char *pcMsg)
{
    const MSG_KEEP_ALIVE_REQ *req = (const MSG_KEEP_ALIVE_REQ *)pcMsg;
    if(!req)
    {
        log_error("msg handle empty!");
        return -1;
    }
    if(ntohs(req->header.length) < sizeof(MSG_KEEP_ALIVE_REQ) - MSG_HEADER_LEN)
    {
        log_error("login message length not enough!");
        return -1;
    }

    return 0;
}

static MSG_PROC_MAP g_msgProcs[] =
{
    {CMD_LOGIN,             sync_login},
    {CMD_NEW_CFG,           sync_newCfg},
    {CMD_KEEP_ALIVE,        sync_keepAlive}
};

int handle_one_msg(int iSockFd, const char *pcMsg)
{
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    for(int i = 0; i < sizeof(g_msgProcs) / sizeof(g_msgProcs[0]); i++)
    {
        if(g_msgProcs[i].cmd == pcMsgHeader->cmd)
        {
            MSG_PROC pfn = g_msgProcs[i].pfn;
            if(pfn)
            {
                return pfn(iSockFd, pcMsg);
            }
        }
    }

    return -1;
}

int handle_sync_msg(int iSockFd, const char *pcMsg, int iMsgLen)
{
    const MSG_HEADER *pcMsgHeader = (const MSG_HEADER *)pcMsg;

    if(iMsgLen < MSG_HEADER_LEN)
    {
        log_error("sync message length not enough(%u<%u)", iMsgLen, MSG_HEADER_LEN);
        return -1;
    }

    int iLeftLen = iMsgLen;
    while(iLeftLen >= ntohs(pcMsgHeader->length) + MSG_HEADER_LEN)
    {
        const unsigned char *status = (const unsigned char *)(&(pcMsgHeader->signature));
        if((status[0] != START_FLAG / 0x100) || (status[1] != START_FLAG % 0x100))
        {
            log_error("receive message header signature error:%x", (unsigned)ntohs(pcMsgHeader->signature));
            return -1;
        }
        handle_one_msg(iSockFd, (const char *)pcMsgHeader);
        iLeftLen = iLeftLen - MSG_HEADER_LEN - ntohs(pcMsgHeader->length);
        pcMsgHeader = (const MSG_HEADER *)(pcMsg + iMsgLen - iLeftLen);
    }

    return 0;
}

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

    /* add slave sync login timerfd to epoll */
    g_iLoginTimerFd = timer_create();
    if(g_iLoginTimerFd < 0)
    {
        log_error("login timer create error(%d)!", g_iLoginTimerFd);
        return (void *)-1;
    }
    iRet = timer_start(g_iLoginTimerFd, 1000 * 10); //10s
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return (void *)-1;
    }
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = g_iLoginTimerFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    iRet = epoll_ctl(iSyncEpollFd, EPOLL_CTL_ADD, g_iLoginTimerFd, &stEvent);
    if(iRet < 0)
    {
        log_error("epoll add g_iLoginTimerFd error(%d)!", iRet);
        return (void *)-1;
    }
    srand((int)time(0));
    g_slaveSpecifyNum = (char)(rand() % 0x100);
    g_slaveSyncStatus = STATUS_LOGIN;//change status into STATUS_LOGIN


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

                if(uiEventsFlag & SLAVE_EVENT_QUEUE_PUSH) //set the flag when sync thread find no keep alive ack
                {
                    log_info("Get SLAVE_EVENT_QUEUE_PUSH.");
                }

            }//if iSyncEventFd
            else if(stEvents[i].data.fd == g_iLoginTimerFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event g_iLoginTimerFd.");
                if(g_slaveSyncStatus == STATUS_LOGIN)
                {
                    //send login msg
                    MSG_LOGIN_REQ *pLoginReq = (MSG_LOGIN_REQ *)alloc_slave_reqMsg(CMD_LOGIN, sizeof(MSG_LOGIN_REQ));
                    if(pLoginReq == NULL)
                    {
                        log_error("alloc_slave_reqMsg MSG_LOGIN_REQ error!");
                        return (void *)-1;
                    }
                    pLoginReq->synFlag = 1;//the first one in three-way handshake
                    pLoginReq->ackFlag = 0;
                    pLoginReq->specifyNum = g_slaveSpecifyNum;
                    if(write(iSyncToSyncSockFd, pLoginReq, sizeof(MSG_LOGIN_REQ)) < 0) //after connect, write = send
                    {
                        log_debug("Send to SLAVE SYNC failed!");
                    }
                }

                iRet = timer_start(g_iLoginTimerFd, 1000 * 10); //10s
                if(iRet < 0)
                {
                    log_error("login timer start error(%d)!", iRet);
                    return (void *)-1;
                }
            }//else if g_iLoginTimerFd
            else if(stEvents[i].data.fd == iSyncToSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                log_debug("epoll event iSyncToSyncSockFd.");

                /* wait for socket msg */
                memset(acBuffer, 0, MAX_BUFFER_SIZE);
                if((iBufferSize = read(iSyncToSyncSockFd, acBuffer, MAX_BUFFER_SIZE)) > 0) //after connect, read = recv
                {
                    log_hex(acBuffer, iBufferSize);

                    handle_sync_msg(iSyncToSyncSockFd, acBuffer, iBufferSize);
                }

            }//else if iSyncToSyncSockFd
        }//for
    }//while

    return (void *)0;
}



