#include <sys/timerfd.h> //for timerfd
#include <string.h> //for memset
#include <unistd.h> //for close
#include "timer.h"
#include "log.h"

DWORD timer::init()
{
    INT iRet = timerfd_create(CLOCK_MONOTONIC, 0); //absolute time from when the system on
    if(iRet < 0)
    {
        log_error(byLogNum, "timerfd create error!");
        return FAILE;
    }

    dwTimerFd = iRet;

    log_info(byLogNum, "timerfd(%u) create ok.", dwTimerFd);
    return SUCCESS;
}

DWORD timer::start(DWORD dwMS)
{
    if(dwMS == 0) 
    {
        log_error(byLogNum, "timer start dwMS(%u) error!", dwMS);
        return FAILE;
    }

    struct itimerspec stTimerSpec;
    memset(&stTimerSpec, 0, sizeof(struct itimerspec));
    stTimerSpec.it_value.tv_sec = dwMS / 1000; // tv_sec 最大值 0x7fffffff
    stTimerSpec.it_value.tv_nsec = dwMS % 1000 * 1000000;
    stTimerSpec.it_interval.tv_sec = 0;
    stTimerSpec.it_interval.tv_nsec = 0;

    if(timerfd_settime(dwTimerFd, 0, &stTimerSpec, NULL) < 0) //0 for a time relative to the current value of the clock
    {
        log_error(byLogNum, "timerfd settime error!");
        return FAILE;
    }

    log_info(byLogNum, "timerfd(%u) settime ok.", dwTimerFd);
    return SUCCESS;
}

DWORD timer::stop()
{
    struct itimerspec stTimerSpec;
    memset(&stTimerSpec, 0, sizeof(struct itimerspec));
    if(timerfd_settime(dwTimerFd, 0, &stTimerSpec, NULL) < 0)
    {
        log_error(byLogNum, "timerfd settime error!");
        return FAILE;
    }

    return SUCCESS;
}

DWORD timer::get(DWORD *pdwMS)
{
    struct itimerspec stTimerSpec;
    memset(&stTimerSpec, 0, sizeof(struct itimerspec));

    if(timerfd_gettime(dwTimerFd, &stTimerSpec) < 0)
    {
        log_error(byLogNum, "timerfd gettime error!");
        return FAILE;
    }

    *pdwMS = stTimerSpec.it_value.tv_sec * 1000 + stTimerSpec.it_value.tv_nsec / 1000000;
    return SUCCESS;
}

DWORD timer::free()
{
    close(dwTimerFd);

    return SUCCESS;
}

