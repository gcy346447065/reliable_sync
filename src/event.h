#ifndef _EVENT_H_
#define _EVENT_H_

#include "macro.h"

#define MASTER_EVENT_NULL                   0x0000
#define MASTER_EVENT_DATA_BATCH             0x0001
#define MASTER_EVENT_DATA_INSTANT           0x0002
#define MASTER_EVENT_DATA_WAITED            0x0004
#define MASTER_EVENT_SLAVE_RESTART          0x0008
#define MASTER_EVENT_CHECKALIVE_TIMER       0x0010


//这里实例化其实都对应的是同一个系统资源
class event
{

public:
    DWORD dwEventFd;
    BYTE byLogNum;

    event(BYTE byNum)
    {
        byLogNum = byNum;
    }

    DWORD init(DWORD dwInitVal);
    DWORD getEventFlags(QWORD *pqwEventFlag);
    DWORD setEventFlags(QWORD qwEventFlag);
    DWORD resetEventFlags(QWORD qwEventFlag);
};

#endif //_EVENT_H_

