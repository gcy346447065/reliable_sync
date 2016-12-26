#ifndef _SYNC_H_
#define _SYNC_H_

#include "queue.h"

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_NEW_CFG = 2
};

static char g_masterSyncStatus = STATUS_INIT;
static char g_loginRspSeq = 0;
static char g_masterSpecifyNum = 0;
static char g_slaveSpecifyNum = 0;

static short g_newCfgID = 0;

static int g_iKeepaliveTimerFd = 0;
static int g_iSyncSockFd = 0;
static int g_iLoginTimerFd = 0;

struct sync_struct
{
    int iMainEventFd;
    int iSyncEventFd;
    stQueue *pstInstantQueue;
    stQueue *pstWaitedQueue;
};


void *master_sync(void *arg);




#endif //_SYNC_H_