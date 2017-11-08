#include <sys/epoll.h> //for epoll
#include <unistd.h> //for close
#include "log.h"
#include "vos.h"

DWORD vos::vos_Init()
{
    /* epoll create */
    INT iRet = epoll_create(MAX_EPOLL_NUM);
    if(iRet < 0)
    {
        log_error(byLogNum, "epoll create error(%d)!", iRet);
        return FAILE;
    }
    dwEpollFd = iRet;

    return SUCCESS;
}

void vos::vos_free()
{
    close(dwEpollFd);
    return;
}

DWORD vos::vos_RegTask(const CHAR *pcTaskName, DWORD dwTaskEventFd, TASK_FUNC pTaskFunc, void *pArg)
{
    map<const CHAR *, VOS_TASK_S>::iterator itTask = mapTask.find(pcTaskName);
    if(itTask == mapTask.end())//该pcTaskName未出现过
    {
        VOS_TASK_S stVosTask;
        stVosTask.dwTaskEventFd = dwTaskEventFd;//dwTaskEventFd在创建相应事件句柄时确定
        stVosTask.pTaskFunc = pTaskFunc;
        stVosTask.pArg = pArg;

        vos_addEvent(dwTaskEventFd, 0);
        mapTask.insert(make_pair(pcTaskName, stVosTask));
    }
	else//该pcTaskName已经注册过
	{}
    
    //log_debug(byLogNum, "vos_RegTask ok.");
    return SUCCESS;
}

DWORD vos::vos_DisregTask(const CHAR *pcTaskName)
{
    map<const CHAR *, VOS_TASK_S>::iterator itTask = mapTask.find(pcTaskName);
    if(itTask != mapTask.end())
    {
        vos_deleteEvent(itTask->second.dwTaskEventFd);
        mapTask.erase(itTask);
    }

    //log_debug(byLogNum, "vos_DisregTask ok.");
	return SUCCESS;
}

DWORD vos::vos_EpollWait()
{
    log_info(byLogNum, "epoll_wait begin, dwEpollFd(%u)...", dwEpollFd);

    struct epoll_event stEvents[MAX_EPOLL_NUM];
    while(TRUE)
    {
        INT iEpollNum = epoll_wait(dwEpollFd, stEvents, MAX_EPOLL_NUM, 500); //wait 500ms or get event
        for(INT i = 0; i < iEpollNum; i++)
        {
            //log_debug(byLogNum, "stEvents[i].data.fd(%d).", stEvents[i].data.fd);
            for(map<const CHAR *, VOS_TASK_S>::iterator itTask = mapTask.begin(); itTask != mapTask.end(); itTask++)
            {
                //log_debug(byLogNum, "itTask->second.dwTaskEventFd(%d).", itTask->second.dwTaskEventFd);
                if((DWORD)stEvents[i].data.fd == itTask->second.dwTaskEventFd)
                {
                    itTask->second.pTaskFunc(itTask->second.pArg);
                }
            }
        }
    }

    return FAILE;
}

DWORD vos::vos_addEvent(DWORD dwEventFd, bool bETorLT)
{
    struct epoll_event stEvent = {0};
    stEvent.data.fd = dwEventFd;
    stEvent.events = EPOLLIN; //epoll for read, level triggered
    INT iRet = epoll_ctl(dwEpollFd, EPOLL_CTL_ADD, dwEventFd, &stEvent);
    if(iRet < 0)
    {
        return FAILE;
    }

    //log_info(byLogNum, "vos_addEvent ok, EpollFd(%d), EventFd(%d).", dwEpollFd, dwEventFd);
    return SUCCESS;
}

DWORD vos::vos_deleteEvent(DWORD dwEventFd)
{
    struct epoll_event stEvent = {0};
    stEvent.data.fd = dwEventFd;
    stEvent.events = EPOLLIN; //epoll for read, level triggered
    INT iRet = epoll_ctl(dwEpollFd, EPOLL_CTL_DEL, dwEventFd, &stEvent);
    if(iRet < 0)
    {
        return FAILE;
    }

    //log_info(byLogNum, "vos_deleteEvent ok, EpollFd(%d), EventFd(%d).", dwEpollFd, dwEventFd);
    return SUCCESS;
}

