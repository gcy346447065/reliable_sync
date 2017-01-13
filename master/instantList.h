#ifndef _INSTANT_LIST_H_
#define _INSTANT_LIST_H_

#include <pthread.h>
#include "protocol.h"

typedef struct InstantNode
{
    char cFindTimers;
    char cSendTimers;
    unsigned int uiInstantID;
    int iDataLen;
    void *pData;
    struct InstantNode *pPrev;
    struct InstantNode *pNext;
}stInstantNode;

typedef struct InstantList
{
    stInstantNode *pFront;
    stInstantNode *pNew;
    stInstantNode *pRear;
    unsigned int uiListSize;
    unsigned int uiNewSize;
    pthread_mutex_t pMutex;
}stInstantList;


stInstantList *instantList_create(void);
void instantList_free(stInstantList *pstInstanList);
void instantList_clean(stInstantList *pstInstanList);

int instantList_push(stInstantList *pstInstanList, void *pData, int iDataLen);
int instantList_delete(stInstantList *pstInstanList, stInstantNode *pNode);//调用此接口需要保证pNode一定在pstInstantList中！
int instantList_moveNew(stInstantList *pstInstanList);
stInstantNode *instantList_find(stInstantList *pstInstanList, unsigned int uiTargetDataID);

int instantList_traverseAndResend(stInstantList *pstInstanList);

#endif //_INSTANT_LIST_H_
