#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include "log.h"
#include "event.h"
#include "vos.h"

DWORD vos::VOS_Init()
{
    /* epoll create */
    INT iRet = epoll_create(MAX_EPOLL_NUM);
    if(iRet < 0)
    {
        log_error("epoll create error(%d)!", iRet);
        return FAILE;
    }

    g_dwEpollFd = iRet;
    g_dwMapCount = 0;
    g_dwTaskMacros = 0;

    log_info("VOS_Init ok, EpollFd(%lu).", g_dwEpollFd);
    return SUCCESS;
}

static DWORD __add_event_ET(DWORD dwEpollFd, DWORD dwEventFd)
{
    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = dwEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    INT iRet = epoll_ctl(dwEpollFd, EPOLL_CTL_ADD, dwEventFd, &stEvent);
    if(iRet < 0)
    {
        return FAILE;
    }

    //log_info("__add_event_ET ok, EpollFd(%d), EventFd(%d).", dwEpollFd, dwEventFd);
    return SUCCESS;
}

static DWORD __add_event_LT(DWORD dwEpollFd, DWORD dwEventFd)
{
    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = dwEventFd;
    stEvent.events = EPOLLIN; //epoll for read, level triggered
    INT iRet = epoll_ctl(dwEpollFd, EPOLL_CTL_ADD, dwEventFd, &stEvent);
    if(iRet < 0)
    {
        return FAILE;
    }

    //log_info("__add_event_LT ok, EpollFd(%d), EventFd(%d).", dwEpollFd, dwEventFd);
    return SUCCESS;
}

//需要先注册macro与eventFd的关系，再注册macro与func的关系(同时会通过macro查找到eventFd并添加到vos中)
DWORD vos::VOS_RegTaskEventFd(DWORD dwTaskMacro, DWORD dwTaskEventFd)
{
    if(!(dwTaskMacro & g_dwTaskMacros))//该位上的task未注册过
    {
        g_dwTaskMacros |= dwTaskMacro;//记录下该task已注册过
        //记录下macro、event对应关系方便查找
        g_vosTaskMap[g_dwMapCount].dwTaskMacro = dwTaskMacro;
        g_vosTaskMap[g_dwMapCount].dwTaskEventFd = dwTaskEventFd;
        g_dwMapCount++;
    }
    
    //log_debug("dwTaskMacro(%lu), g_dwTaskMacros(%lu)", dwTaskMacro, g_dwTaskMacros);
    //log_debug("g_dwMapCount(%lu)", g_dwMapCount);
    //log_debug("VOS_RegTaskEventFd ok.");
    return SUCCESS;
}

//本来是第一个参数是const CHAR *pszTaskName，为了方便比对，故而修改成宏定义
DWORD vos::VOS_RegTaskFunc(DWORD dwTaskMacro, TASK_FUNC taskFunc, void *pArg)
{
    DWORD dwTaskEventFd = 0;

    if(dwTaskMacro & g_dwTaskMacros)//该位上的task已注册过
    {
        //log_debug("g_dwMapCount(%lu)", g_dwMapCount);
        for(UINT i = 0; i < g_dwMapCount; i++)
        {
            //log_debug("dwTaskMacro(%lu), dwTaskEventFd(%lu)", g_vosTaskMap[i].dwTaskMacro, g_vosTaskMap[i].dwTaskEventFd);
            if(g_vosTaskMap[i].dwTaskMacro == dwTaskMacro)
            {
                dwTaskEventFd = g_vosTaskMap[i].dwTaskEventFd;//前面已经记录过此关系
                g_vosTaskMap[i].func = taskFunc;
            }
        }

        //添加此条pTaskEvent到g_dwEpollFd
        if(dwTaskMacro == VOS_TASK_MASTER_MAILBOX || dwTaskMacro == VOS_TASK_SLAVE_MAILBOX)
        {
            __add_event_LT(g_dwEpollFd, dwTaskEventFd);//level triggered
        }
        else
        {
            __add_event_ET(g_dwEpollFd, dwTaskEventFd);//edge triggered
        }
    }

    //log_debug("dwTaskMacro(%lu), g_dwTaskMacros(%lu)", dwTaskMacro, g_dwTaskMacros);
    //log_debug("VOS_RegTaskFunc ok.");
    return SUCCESS;
}

DWORD vos::VOS_ReceiveEvent(DWORD dwTargetEvents, DWORD dwEvAny, 
    DWORD dwWaitForever, DWORD *pdwEvent)
{
    //实际没有用到

    return SUCCESS;
}

DWORD vos::VOS_EpollWait()
{
    log_info("epoll_wait begin, EpollFd(%lu)...", g_dwEpollFd);

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(1)
    {
        INT iEpollNum = epoll_wait(g_dwEpollFd, stEvents, MAX_EPOLL_NUM, 0); //wait 500ms or get event
        for(INT i = 0; i < iEpollNum; i++)
        {
            //log_debug("stEvents[i].data.fd(%d).", stEvents[i].data.fd);
            for(UINT j = 0; j < g_dwMapCount; j++)
            {
                //log_debug("g_vosTaskMap[j].dwTaskEventFd(%d).", g_vosTaskMap[j].dwTaskEventFd);
                if((DWORD)stEvents[i].data.fd == g_vosTaskMap[j].dwTaskEventFd)
                {
                    //log_debug("epoll_wait find EventFd(%d).", g_vosTaskMap[j].dwTaskEventFd);
                    g_vosTaskMap[j].func(NULL);
                }
            }
        }
    }

    return FAILE;
}

