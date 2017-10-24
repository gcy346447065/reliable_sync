#ifndef _EVENT_H_
#define _EVENT_H_

#include "macro.h"

#define MASTER_EVENT_NULL                  0x0000
#define MASTER_EVENT_KEYIN_INSTANT         0x0001
#define MASTER_EVENT_KEYIN_WAITED          0x0002
#define MASTER_EVENT_SLAVE_RESTART         0x0004
#define MASTER_EVENT_CHECKALIVE_TIMER      0x0008



class event
{

public:
    DWORD g_dwEventFd;

    DWORD init(DWORD dwInitVal);
    DWORD getEventFlags(QWORD *pqwEventFlag);
    DWORD setEventFlags(QWORD qwEventFlag);
    DWORD resetEventFlags(QWORD qwEventFlag);
};

#endif //_EVENT_H_
