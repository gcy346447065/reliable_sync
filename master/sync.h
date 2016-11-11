#ifndef _SYNC_H_
#define _SYNC_H_

enum SEND_METHOD_TO_SYNC
{
    SEND_BATCH,
    SEND_REALTIME_WAITED,
    SEND_REALTIME_INSTANT
};

struct sync_struct
{
    int iSyncEventFd;
};

void *master_sync(void *arg);




#endif //_SYNC_H_