#ifndef _SYNC_H_
#define _SYNC_H_

#include "queue.h"

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_NEWCFG = 2
};

/*
extern char g_cMasterSyncStatus;
extern char g_cLoginRspSeq;
extern char g_cMasterSpecifyID;
extern char g_cSlaveSpecifyID;

extern int g_iNewCfgID;
extern int g_iKeepaliveTimerFd;
extern int g_iSyncSockFd;
extern int g_iLoginTimerFd;

extern stQueue *g_pstInstantQueue;
extern stQueue *g_pstWaitedQueue;
*/

struct sync_struct
{
    int iMainEventFd;
    int iSyncEventFd;
    stQueue *pstInstantQueue;
    stQueue *pstWaitedQueue;
};


void *master_sync(void *arg);




#endif //_SYNC_H_