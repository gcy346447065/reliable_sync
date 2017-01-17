#ifndef _SYNC_H_
#define _SYNC_H_

#include "instantList.h"
#include "waitedList.h"

struct sync_struct
{
    stInstantList *pstInstantList;
    stWaitedList *pstWaitedList;
};

void *slave_sync_thread(void *arg);

#endif //_SYNC_H_
