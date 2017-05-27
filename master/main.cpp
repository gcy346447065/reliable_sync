#include <sys/epoll.h> //for epoll
#include <unistd.h> //for read STDIN_FILENO
#include <stdio.h> //for sscanf sprintf
#include <stdlib.h> //for malloc
#include <string.h> //for memset strstr
#include <errno.h> //for errno
#include <fcntl.h> //for open
#include "macro.h"
#include "log.h"
#include "timer.h"
#include "event.h"
#include "socket.h"
#include "sync.h"
#include "tool.h"
#include "instantList.h"
#include "waitedList.h"

int g_iMainEpollFd = 0;
int g_iMainEventFd = 0;
int g_iSyncEventFd = 0;

static int g_iFileNumInstant = 0;
static int g_iFileNumWaitedBegin = 0;
static int g_iFileNumWaitedEnd = 0;

/*
 * 配置模块调用同步API前要做的初始化操作
 */
int reliable_sync_init(void)
{
    /* list init */
    int iRet = instantList_init();
    if(iRet < 0)
    {
        log_error("instantList_init error!");
        return -1;
    }
    iRet = waitedList_init();
    if(iRet < 0)
    {
        log_error("waitedList_init error!");
        return -1;
    }

    /* epoll create */
    g_iMainEpollFd = epoll_create(MAX_EPOLL_NUM);
    if(g_iMainEpollFd < 0)
    {
        log_error("epoll create error(%d)!", g_iMainEpollFd);
        return -1;
    }

    /* eventfd init */
    g_iMainEventFd = event_init(0); //0 for event flag init value
    if(g_iMainEventFd < 0)
    {
        log_error("event_init error(%d)!", g_iMainEventFd);
        return -1;
    }
    g_iSyncEventFd = event_init(0); //0 for event flag init value
    if(g_iSyncEventFd < 0)
    {
        log_error("event_init error(%d)!", g_iSyncEventFd);
        return -1;
    }

    /* add g_iMainEventFd to g_iMainEpollFd */
    iRet = tool_add_event_to_epoll(g_iMainEpollFd, g_iMainEventFd);
    if(iRet < 0)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", g_iMainEpollFd, g_iMainEventFd, iRet);
        return -1;
    }

    /* add STDIN_FILENO to g_iMainEpollFd */
    iRet = tool_add_event_to_epoll(g_iMainEpollFd, STDIN_FILENO);
    if(iRet < 0)
    {
        log_error("Epoll(%d) add Event(%d) error(%d)!", g_iMainEpollFd, STDIN_FILENO, iRet);
        return -1;
    }

    /* sync pthread create */
    pthread_t SyncThreadId;
    struct sync_struct stSyncStruct;
    iRet = pthread_create(&SyncThreadId, NULL, master_sync_thread, (void *)&stSyncStruct);
    if(iRet < 0)
    {
        log_error("pthread create error(%d)!", iRet);
        return -1;
    }

    return 0;
}

/*
 * 配置模块可调用该函数发送配置
 */
int reliable_sync_send(void *pBuf, int iBufLen, int iMaxPkgLen, void *pDestAddr, int iSendMethod)
{
    if(pBuf == NULL || iBufLen == 0)
    {
        log_error("SendToSync pBuf or iBufLen error!");
        return -1;
    }
    //TODO: iMaxPkgLen, pDestAddr unused

    switch(iSendMethod)
    {
        case SEND_NEWCFG_INSTANT:
            log_info("instantList_push(%d).", iBufLen);
            instantList_push(pBuf, iBufLen);
            break;

        case SEND_NEWCFG_WAITED:
            log_info("waitedList_push(%d).", iBufLen);
            waitedList_push(pBuf, iBufLen);
            break;

        default:
            log_error("iSendMethod:%d.", iSendMethod);
            return -1;
    }

    return 0;
}

int _epoll_stdin(void)
{
    log_info("Get STDIN_FILENO fd.");

    //从控制台读取键入字符串
    char *pcStdinBuf = (char *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcStdinBuf, 0, MAX_STDIN_FILE_LEN);
    int iRet = read(STDIN_FILENO, pcStdinBuf, MAX_STDIN_FILE_LEN);
    if(iRet < 0)
    {
        log_error("read STDIN_FILENO error(%s)!", strerror(errno));
        return -1;
    }
    pcStdinBuf[iRet-1] = '\0'; //-1 for '\n'

    //字符串为"?file%d"时为单个文件instant立即备份
    //字符串为"/file%d:file%d"时为区间内多个文件waited定时定量备份
    if(sscanf(pcStdinBuf, "?file%d", &g_iFileNumInstant) == 1) //"?file2"
    {
        char *pcFilenameInstant = (char *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilenameInstant, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilenameInstant, "file%d", g_iFileNumInstant);
        if((open(pcFilenameInstant, O_RDONLY)) > 0)
        {
            log_info("open INSTANT file ok.");

            iRet = write(STDIN_FILENO, "Send INSTANT to slave.\r\n", strlen("Send INSTANT to slave.\r\n"));
            if(iRet < 0)
            {
                log_error("write STDIN_FILENO error(%s)!", strerror(errno));
                return -1;
            }

            //触发instant键入事件(向sync模块发送单个文件)
            iRet = event_setEventFlags(g_iMainEventFd, MASTER_MAIN_EVENT_KEYIN_INSTANT);
            if(iRet != 0)
            {
                log_error("set event flag MASTER_MAIN_EVENT_KEYIN_INSTANT failed!");
                return -1;
            }
        }
        else
        {
            log_info("open new config file error(%s)!", strerror(errno));
        }

        free(pcFilenameInstant);
    }
    else if(sscanf(pcStdinBuf, "/file%d:%d", &g_iFileNumWaitedBegin, &g_iFileNumWaitedEnd) == 2) //"/file8:10"
    {
        char *pcFilenameBegin = (char *)malloc(MAX_STDIN_FILE_LEN);
        char *pcFilenameEnd = (char *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilenameBegin, 0, MAX_STDIN_FILE_LEN);
        memset(pcFilenameEnd, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilenameBegin, "file%d", g_iFileNumWaitedBegin);
        sprintf(pcFilenameEnd, "file%d", g_iFileNumWaitedEnd);
        if(open(pcFilenameBegin, O_RDONLY) > 0 && open(pcFilenameEnd, O_RDONLY) > 0)
        {
            log_info("open WAITED files ok.");

            iRet = write(STDIN_FILENO, "Send WAITED to slave.\r\n", strlen("Send WAITED to slave.\r\n"));
            if(iRet < 0)
            {
                log_error("write STDIN_FILENO error(%s)!", strerror(errno));
                return -1;
            }

            //触发waited键入事件(向sync模块发送多个文件)
            iRet = event_setEventFlags(g_iMainEventFd, MASTER_MAIN_EVENT_KEYIN_WAITED);
            if(iRet != 0)
            {
                log_error("set event flag MASTER_MAIN_EVENT_KEYIN_WAITED failed!");
                return -1;
            }
        }
        else
        {
            log_info("open new config file error(%s)!", strerror(errno));
        }

        free(pcFilenameBegin);
        free(pcFilenameEnd);
    }
    else if(sscanf(pcStdinBuf, "/file%d", &g_iFileNumWaitedBegin) == 1) //"/file8:10"
    {
        g_iFileNumWaitedEnd = g_iFileNumWaitedBegin;
        char *pcFilenameBegin = (char *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilenameBegin, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilenameBegin, "file%d", g_iFileNumWaitedBegin);
        if(open(pcFilenameBegin, O_RDONLY) > 0)
        {
            log_info("open WAITED files ok.");

            iRet = write(STDIN_FILENO, "Send WAITED to slave.\r\n", strlen("Send WAITED to slave.\r\n"));
            if(iRet < 0)
            {
                log_error("write STDIN_FILENO error(%s)!", strerror(errno));
                return -1;
            }

            //触发waited键入事件(向sync模块发送多个文件)
            iRet = event_setEventFlags(g_iMainEventFd, MASTER_MAIN_EVENT_KEYIN_WAITED);
            if(iRet != 0)
            {
                log_error("set event flag MASTER_MAIN_EVENT_KEYIN_WAITED failed!");
                return -1;
            }
        }
        else
        {
            log_info("open new config file error(%s)!", strerror(errno));
        }

        free(pcFilenameBegin);
    }
    else
    {
        log_info("Unknown command!");
    }

    free(pcStdinBuf);
    return 0;
}

int _epoll_mainEvent(void)
{
    log_info("Get iMainEventFd.");

    //获取事件标志，共有64种事件，可同时触发多个
    uint64_t uiEventsFlag;
    int iRet = event_getEventFlags(g_iMainEventFd, &uiEventsFlag);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }

    //触发instant键入事件，向sync模块发送配置消息
    if(uiEventsFlag & MASTER_MAIN_EVENT_KEYIN_INSTANT)
    {
        log_info("Get MASTER_MAIN_EVENT_KEYIN_INSTANT.");

        char *pcFilenameInstant = (char *)malloc(MAX_STDIN_FILE_LEN);
        memset(pcFilenameInstant, 0, MAX_STDIN_FILE_LEN);
        sprintf(pcFilenameInstant, "file%d", g_iFileNumInstant);

        int iFileInstantFd;
        if((iFileInstantFd = open(pcFilenameInstant, O_RDONLY)) > 0)
        {
            char *pcInstantBuf = (char *)malloc(MAX_PKG_LEN);
            memset(pcInstantBuf, 0, MAX_PKG_LEN);
            iRet = read(iFileInstantFd, pcInstantBuf, MAX_PKG_LEN);
            if(iRet < 0)
            {
                log_error("read iFileInstantFd error(%d)!", iRet);
                return -1;
            }
            int iBufLen = iRet;

            //向sync模块发送instant配置消息
            iRet = reliable_sync_send(pcInstantBuf, iBufLen, MAX_PKG_LEN, NULL, SEND_NEWCFG_INSTANT);
            if(iRet < 0)
            {
                log_info("reliable_sync_send failed(%d)!", iRet);
            }

            //触发sync模块的instant事件
            iRet = event_setEventFlags(g_iSyncEventFd, MASTER_SYNC_EVENT_NEWCFG_INSTANT);
            if(iRet < 0)
            {
                log_error("event_setEventFlags error(%d)!", iRet);
                return -1;
            }

            free(pcInstantBuf);
        }

        free(pcFilenameInstant);
    }

    //触发waited键入事件，循环向sync模块发送配置消息
    if(uiEventsFlag & MASTER_MAIN_EVENT_KEYIN_WAITED) //when find specifyID different, get slave restart event
    {
        log_info("Get MASTER_MAIN_EVENT_KEYIN_WAITED.");

        char *pcWaitedBuf = (char *)malloc(MAX_PKG_LEN);
        char *pcFilenameWaited = (char *)malloc(MAX_STDIN_FILE_LEN);

        for(int i = g_iFileNumWaitedBegin; i <= g_iFileNumWaitedEnd; i++)
        {
            memset(pcFilenameWaited, 0, MAX_STDIN_FILE_LEN);
            sprintf(pcFilenameWaited, "file%d", i);
            int iFileWaitedFd;
            if((iFileWaitedFd = open(pcFilenameWaited, O_RDONLY)) > 0)
            {printf("get into %s\n", pcFilenameWaited);
                int iBufLen;
                do
                {
                    memset(pcWaitedBuf, 0, MAX_PKG_LEN);
                    iRet = read(iFileWaitedFd, pcWaitedBuf, MAX_PKG_LEN);
                    if(iRet < 0)
                    {
                        log_error("read iFileWaitedFd error(%d)!", iRet);
                        return -1;
                    }
                    iBufLen = iRet;

                    iRet = (iBufLen == MAX_PKG_LEN);

                    waitedList_ID(iRet);

                    //向sync模块循环发送waited配置消息
                    iRet = reliable_sync_send(pcWaitedBuf, iBufLen, MAX_PKG_LEN, NULL, SEND_NEWCFG_WAITED);
                    if(iRet < 0)
                    {
                        log_info("reliable_sync_send failed(%d)!", iRet);
                    }
                }while(iBufLen == MAX_PKG_LEN);
                close(iFileWaitedFd);
            }
        }

        free(pcWaitedBuf);
        free(pcFilenameWaited);

        //触发sync模块的waited事件
        iRet = event_setEventFlags(g_iSyncEventFd, MASTER_SYNC_EVENT_NEWCFG_WAITED);
        if(iRet < 0)
        {
            log_error("event_setEventFlags error(%d)!", iRet);
            return -1;
        }printf("get out %s\n", pcFilenameWaited);
    }

    //通过sync模块的keep_alive机制检测到slave有重启，清除链表中残余配置并重新开始批量备份
    if(uiEventsFlag & MASTER_MAIN_EVENT_SLAVE_RESTART)
    {
        log_info("Get MASTER_MAIN_EVENT_SLAVE_RESTART, batch backup.");

        //TO DO: clean the List and batch backup
    }

    //sync模块超时未收到slave的消息，作重启操作？
    if(uiEventsFlag & MASTER_MAIN_EVENT_CHECKALIVE_TIMER)
    {
        log_info("Get MASTER_MAIN_EVENT_CHECKALIVE_TIMER, restart.");

        /* restart */
    }

    return 0;
}

/*
 * 配置模块为实际主线程
 */
int main(int argc, char *argv[])
{
    //现用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_init();

    log_info("MAIN Task Beginning.");

    int iRet = reliable_sync_init();
    if(iRet < 0)
    {
        log_error("reliable_sync_init");
    }

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(1)
    {
        int iEpollNum = epoll_wait(g_iMainEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(int i = 0; i < iEpollNum; i++)
        {
            if(stEvents[i].data.fd == STDIN_FILENO && stEvents[i].events & EPOLLIN)
            {
                //控制台有输入，用于测试时触发下配置
                iRet = _epoll_stdin();
                if(iRet < 0)
                {
                    log_warning("_epoll_stdin failed!");
                }
            }
            else if(stEvents[i].data.fd == g_iMainEventFd && stEvents[i].events & EPOLLIN)
            {
                //接收到新事件，主要用于配置模块与同步模块的通讯
                iRet = _epoll_mainEvent();
                if(iRet < 0)
                {
                    log_warning("_epoll_mainEvent failed!");
                }
            }
        }
    }

    /* free all */
    log_info("MAIN Task Ending.");
    log_free();
    close(g_iMainEpollFd);
    close(g_iMainEventFd);
    return 0;
}

