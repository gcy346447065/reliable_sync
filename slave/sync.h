#ifndef _SYNC_H_
#define _SYNC_H_


enum SEND_METHOD_TO_SYNC
{
    SEND_SET_SYNC_TIMER,
    SEND_NEWCFG_WAITED,
    SEND_NEWCFG_INSTANT
};

struct sync_struct
{
    int iMainEventFd;
    int iSyncEventFd;
};


void *slave_sync(void *arg);




#endif //_SYNC_H_