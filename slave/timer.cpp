#include <sys/timerfd.h>
#include <string.h> //for memset
#include <unistd.h> //for close
#include "timer.h"
#include "log.h"

int timer_create(void)
{
    int iTimerFd = timerfd_create(CLOCK_MONOTONIC, 0); //absolute time from when the system on
    if(iTimerFd < 0)
    {
        log_error("timerfd create error!");
        return -1;
    }

    return iTimerFd;
}

int timer_start(int iTimerFd, int iMS)
{
    if(iMS <= 0)
    {
        log_error("timer start iMS(%d) error!", iMS);
        return -1;
    }

    struct itimerspec iNewTimerSpec;
    memset(&iNewTimerSpec, 0, sizeof(struct itimerspec));
    iNewTimerSpec.it_interval.tv_sec = iMS/1000;
    iNewTimerSpec.it_interval.tv_nsec = iMS%1000*1000000;
    iNewTimerSpec.it_value.tv_sec = iMS/1000;
    iNewTimerSpec.it_value.tv_nsec = iMS%1000*1000000;

    if(timerfd_settime(iTimerFd, 0, &iNewTimerSpec, NULL) < 0) //0 for a time relative to the current value of the clock
    {
        log_error("timerfd settime error!");
        return -2;
    }

    return 0;
}

//必须要read timerFd，否则会连续触发epoll
int timer_read(int iTimerFd)
{
    unsigned long int ulExp;
    int iRet = read(iTimerFd, &ulExp, sizeof(ulExp));
    if(iRet != sizeof(ulExp))
    {
        return -1;
    }
    
    return 0;
}

int timer_stop(int iTimerFd)
{
    struct itimerspec iNewTimerSpec;
    memset(&iNewTimerSpec, 0, sizeof(struct itimerspec));
    if(timerfd_settime(iTimerFd, 0, &iNewTimerSpec, NULL) < 0)
    {
        log_error("timerfd settime error!");
        return -1;
    }

    return 0;
}

int timer_close(int iTimerFd)
{
    close(iTimerFd);
    return 0;
}
