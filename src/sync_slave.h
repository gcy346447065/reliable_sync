#ifndef _SYNC_SLAVE_H_
#define _SYNC_SLAVE_H_

#include "macro.h"
#include "event.h"

/*
 * 接受端
 */
class sync_slave
{

public:
    DWORD init(DWORD dwLclAddr, DWORD dwOppAddr);

};

#endif //_SYNC_SLAVE_H_
