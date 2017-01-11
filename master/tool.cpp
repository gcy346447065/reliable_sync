#include <sys/epoll.h> //for epoll
#include <string.h> //for memset strstr
#include "tool.h"

int tool_add_event_to_epoll(int iEpollFd, int iEventFd)
{
    struct epoll_event stEvent;
    memset(&stEvent, 0, sizeof(struct epoll_event));
    stEvent.data.fd = iEventFd;
    stEvent.events = EPOLLIN | EPOLLET; //epoll for read, edge triggered
    int iRet = epoll_ctl(iEpollFd, EPOLL_CTL_ADD, iEventFd, &stEvent);
    if(iRet < 0)
    {
        return -1;
    }

    return 0;
}
