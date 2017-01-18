#include <netinet/in.h> //for sockaddr_in
#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
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
#include "send.h"
#include "recv.h"
#include "protocol.h"
#include "tool.h"
#include "instantList.h"
#include "waitedList.h"

char g_cSlaveSyncStatus = STATUS_INIT;
char g_cMasterSpecifyID = 0;
char g_cSlaveSpecifyID = 0;

int g_iSyncEpollFd = 0;
int g_iSyncSockFd = 0;
int g_iLoginSynTimerFd = 0;
int g_iKeepaliveTimerFd = 0;
int g_iCheckaliveTimerFd = 0;

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

    //��g_iSyncEventFd��ӵ�epoll�У�����main�̻߳���sync�����¼���������Ҫ��main����
    int iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iSyncEventFd);
    if(iRet < 0)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", g_iSyncEpollFd, g_iSyncEventFd, iRet);
        return -1;
    }

    //��g_iSyncSockFd��ӵ�epoll��
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

    //�յ�master������syn��¼�������˶�ʱ��������ѭ������SynAck��¼��
    g_iLoginSynTimerFd = timer_create();
    if(g_iLoginSynTimerFd < 0)
    {
        log_error("sync timer create error(%d)!", g_iLoginSynTimerFd);
        return -1;
    }
    iRet = tool_add_event_to_epoll(g_iSyncEpollFd, g_iLoginSynTimerFd);
    if(iRet < 0)
    {
        log_error("epoll add g_iLoginSynTimerFd error(%d)!", iRet);
        return -1;
    }
    srand((int)time(0));
    g_cSlaveSpecifyID = (char)(rand() % 0x100);//�����豸ʶ��ID

    iRet = timer_start(g_iLoginSynTimerFd, LOGIN_TIMER_VALUE); //10s
    if(iRet < 0)
    {
        log_error("sync timer start error(%d)!", iRet);
        return -1;
    }
    g_cSlaveSyncStatus = STATUS_LOGIN;//change status into STATUS_LOGIN
    log_debug("timer_start g_iLoginSynTimerFd ok.");

    //����һ����Ϣ�������˶�ʱ��������һ��ʱ������Ϣ����ʱ�����ӱ���
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

    //����һ����Ϣ�������˶�ʱ��������һ��ʱ��δ�յ���Ϣ˵���Զ˹���
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

    return 0;
}

int _epoll_syncSocket(void)
{
    log_info("Get g_iSyncSockFd.");

    int iRet = 0;
    int iBufferSize = 0;
    char *pcBuffer = (char *)malloc(MAX_BUFFER_SIZE);
    memset(pcBuffer, 0, MAX_BUFFER_SIZE);
    if((iBufferSize = recvFromMasterSync(g_iSyncSockFd, pcBuffer, MAX_BUFFER_SIZE)) > 0)
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

    //��ȡ�¼���־������64���¼�����ͬʱ�������
    uint64_t uiEventsFlag;
    int iRet = event_getEventFlags(g_iSyncEventFd, &uiEventsFlag);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }

    if(uiEventsFlag & SLAVE_SYNC_EVENT_NULL)
    {
        //
    }

    return 0;
}

int _epoll_loginSynTimer(void)
{
    log_info("Get g_iLoginSynTimerFd.");

    if(g_cSlaveSyncStatus == STATUS_LOGIN)
    {
        //send login msg
        MSG_LOGIN_REQ *req = (MSG_LOGIN_REQ *)alloc_slave_reqMsg(CMD_LOGIN);
        if(req == NULL)
        {
            log_error("alloc_slave_reqMsg error!");
            return -1;
        }
        req->cSynFlag = 1;//the first one in three-way handshake
        req->cAckFlag = 0;
        req->cSpecifyID = g_cSlaveSpecifyID;
        if(sendToMasterSync(g_iSyncSockFd, req, sizeof(MSG_LOGIN_REQ)) < 0)
        {
            log_debug("Send to MASTER SYNC failed!");
        }
    }

    int iRet = timer_start(g_iLoginSynTimerFd, LOGIN_TIMER_VALUE); //10s
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

    if(g_cSlaveSyncStatus == STATUS_NEWCFG)
    {
        //no msg sent for a while, send keep alive msg
        MSG_KEEP_ALIVE_REQ *req = (MSG_KEEP_ALIVE_REQ *)alloc_slave_reqMsg(CMD_KEEP_ALIVE);
        if(req == NULL)
        {
            log_error("alloc_master_reqMsg error!");
            return -1;
        }
        req->cSpecifyID = g_cSlaveSpecifyID;

        if(sendToMasterSync(g_iSyncSockFd, req, sizeof(MSG_KEEP_ALIVE_REQ)) < 0)
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
    if(g_cSlaveSyncStatus == STATUS_NEWCFG)
    {
        //no msg recived for a while, send event to main to restart
        iRet = event_setEventFlags(g_iMainEventFd, SLAVE_MAIN_EVENT_CHECKALIVE_TIMER);
        if(iRet < 0)
        {
            log_error("set g_iMainEventFd SLAVE_MAIN_EVENT_CHECKALIVE_TIMER error(%d)!", iRet);
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

void *slave_sync_thread(void *arg)
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
        /* epoll wail */
        int iEpollNum = epoll_wait(g_iSyncEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == g_iSyncSockFd && stEvents[i].events & EPOLLIN)
            {
                //�յ����ݰ�
                iRet = _epoll_syncSocket();
                if(iRet < 0)
                {
                    log_warning("_epoll_syncSocket failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iSyncEventFd && stEvents[i].events & EPOLLIN)
            {
                //�¼�
                iRet = _epoll_syncEvent();
                if(iRet < 0)
                {
                    log_warning("_epoll_syncEvent failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iLoginSynTimerFd && stEvents[i].events & EPOLLIN)
            {
                //ѭ������login SYN��Ϣ
                iRet = _epoll_loginSynTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_loginSynTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iKeepaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                //����
                iRet = _epoll_keepaliveTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_keepaliveTimer failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iCheckaliveTimerFd && stEvents[i].events & EPOLLIN)
            {
                //���
                iRet = _epoll_checkaliveTimer();
                if(iRet < 0)
                {
                    log_warning("_epoll_checkaliveTimer failed!");
                }
            }
        }//for
    }//while

    return (void *)0;
}



