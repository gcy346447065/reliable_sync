#ifndef _TIMER_H_
#define _TIMER_H_

#include "macro.h"

/*
 * 
 */
class timer
{

public:
    DWORD dwTimerFd;

    DWORD init();
    DWORD start(DWORD dwMS);
    DWORD stop();
    DWORD get(DWORD *pdwMS);
    DWORD free();

};

#endif //_TIMER_H_

