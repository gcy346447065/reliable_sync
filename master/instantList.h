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


int instantList_init(void);
void instantList_free(void);
void instantList_clean(void);

int instantList_push(void *pData, int iDataLen);
int instantList_delete(stInstantNode *pNode);//调用此接口需要保证pNode一定在pstInstantList中！
int instantList_moveNew(void);
stInstantNode *instantList_find(unsigned int uiTargetDataID);

unsigned int instantList_getNewSize(void);
unsigned int instantList_getListSize(void);
stInstantNode *instantList_getFrontNode(void);
stInstantNode *instantList_getRearNode(void);
stInstantNode *instantList_getNewNode(void);

int instantList_traverseAndResend(void);

#endif //_INSTANT_LIST_H_
