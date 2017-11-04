#include <sys/eventfd.h> //for eventfd
#include <unistd.h> //for read
#include <errno.h> //for errno
#include <string.h> //for strerror
#include "event.h"
#include "log.h"

DWORD event::init(DWORD dwInitVal)
{
    INT iRet = eventfd(dwInitVal, EFD_NONBLOCK); //iInitVal(0) for event flag init value, EFD_NONBLOCK for reading zero no block
    if(iRet < 0)
    {
        log_error(byLogNum, "eventfd error(%d)!", iRet);
        return FAILE;
    }

    dwEventFd = iRet;
    return SUCCESS;
}

DWORD event::getEventFlags(QWORD *pqwEventFlag)
{
    INT iRet = read(dwEventFd, pqwEventFlag, sizeof(QWORD));
    if(iRet != sizeof(QWORD))
    {
        if(errno == EAGAIN) //EAGAIN for pqwEventFlag zero
        {
            *pqwEventFlag = 0;
            return SUCCESS;
        }
        else
        {
            log_error(byLogNum, "read g_dwEventFd error(%d,%s)!", errno, strerror(errno)); 
            return FAILE;
        }
    }

    return SUCCESS;
}

DWORD event::setEventFlags(QWORD qwEventFlag)
{
    QWORD qwEventRead;
    INT iRet = getEventFlags(&qwEventRead);
    if(iRet < 0)
    {
        log_error(byLogNum, "getEventFlags error(%d)!", iRet);
        return FAILE;
    }

    if(qwEventRead & qwEventFlag)
    {
        log_debug(byLogNum, "qwEventRead(%llx) has been setted at qwEventFlag(%llx).", qwEventRead, qwEventFlag);
        return SUCCESS;
    }
    
    QWORD qwEventWrite = qwEventRead | qwEventFlag;
    iRet = write(dwEventFd, &qwEventWrite, sizeof(QWORD));
    if(iRet != sizeof(QWORD))
    {
        log_error(byLogNum, "write g_dwEventFd error(%s)!", strerror(errno));
        return FAILE;
    }

    return SUCCESS;
}

DWORD event::resetEventFlags(QWORD qwEventFlag)
{
    QWORD qwEventRead;
    INT iRet = getEventFlags(&qwEventRead);
    if(iRet < 0)
    {
        log_error(byLogNum, "getEventFlags error(%d)!", iRet);
        return FAILE;
    }

    if(~qwEventRead & qwEventFlag)
    {
        log_debug(byLogNum, "qwEventRead(%llx) has been resetted at qwEventFlag(%llx).", qwEventRead, qwEventFlag);
        return SUCCESS;
    }
    
    QWORD qwEventWrite = qwEventRead & ~qwEventFlag;
    iRet = write(dwEventFd, &qwEventWrite, sizeof(QWORD));
    if(iRet != sizeof(QWORD))
    {
        log_error(byLogNum, "write g_dwEventFd error(%s)!", strerror(errno));
        return FAILE;
    }

    return SUCCESS;
}

