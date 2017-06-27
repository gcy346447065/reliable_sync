#include <sys/eventfd.h>
#include <unistd.h> //for read
#include <errno.h> //for errno
#include <string.h>
#include "event.h"
#include "log.h"

int event_init(unsigned int iInitVal)
{
    int iEventFd = eventfd(iInitVal, EFD_NONBLOCK); //iInitVal(0) for event flag init value, EFD_NONBLOCK for reading zero no block
    if(iEventFd < 0)
    {
        log_error("eventfd error(%d)!", iEventFd);
        return -1;
    }

    return iEventFd;
}

int event_getEventFlags(int iEventFd, uint64_t *puiEventRead)
{
    int iRet = read(iEventFd, puiEventRead, sizeof(uint64_t));
    if(iRet == EAGAIN) 
    {
        *puiEventRead = 0;
    }
    else if(iRet != sizeof(uint64_t))
    {
        if(errno == EAGAIN) //EAGAIN for puiEventRead zero
        {
            *puiEventRead = 0;
            return 0;
        }
        else
        {
            log_error("read iEventFd error(%d,%s)!", errno, strerror(errno)); 
            return -1;
        }
    }
}

int event_setEventFlags(int iEventFd, uint64_t uiEventFlag)
{
    uint64_t uiEventRead;
    int iRet = event_getEventFlags(iEventFd, &uiEventRead);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }
    
    uint64_t uiEventWrite = uiEventRead | uiEventFlag;
    iRet = write(iEventFd, &uiEventWrite, sizeof(uint64_t));
    if(iRet != sizeof(uint64_t))
    {
        log_error("write iEventFd error(%s)!", strerror(errno));
        return -2;
    }

    return 0;
}

int event_resetEventFlags(int iEventFd, uint64_t uiEventFlag)
{
    uint64_t uiEventRead;
    int iRet = event_getEventFlags(iEventFd, &uiEventRead);
    if(iRet < 0)
    {
        log_error("event_getEventFlags error(%d)!", iRet);
        return -1;
    }

    uint64_t uiEventWrite = uiEventRead & ~uiEventFlag;
    iRet = write(iEventFd, &uiEventWrite, sizeof(uint64_t));
    if(iRet != sizeof(uint64_t))
    {
        log_error("write iEventFd error(%s)!", strerror(errno));
        return -2;
    }

    return 0;
}


