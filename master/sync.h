#ifndef _SYNC_H_
#define _SYNC_H_

#include "queue.h"



struct sync_struct
{
    int iMainEventFd;
    int iSyncEventFd;
    stQueue *pstInstantQueue;
    stQueue *pstWaitedQueue;
};


void *master_sync(void *arg);




#endif //_SYNC_H_