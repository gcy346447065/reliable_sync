#ifndef _SYNC_H_
#define _SYNC_H_

struct sync_struct
{
    int iMainEventFd;
    int iSyncEventFd;
};

void *master_sync_thread(void *arg);

#endif //_SYNC_H_
