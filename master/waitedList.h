#ifndef _LIST_H_
#define _LIST_H_

#include <pthread.h>
#include "protocol.h"

typedef struct WaitedNode
{
    char cSendTimers;
    unsigned int uiWaitedID;
    int iDataLen;
    void *pData;
    struct WaitedNode *pPrev;
    struct WaitedNode *pNext;
}stWaitedNode;

typedef struct WaitedList
{
    stWaitedNode *pFront;
    stWaitedNode *pRear;
    unsigned int uiListSize; //链表中的节点个数
    unsigned int uiMsgLen; //如果将链表中的所有节点以MSG_NEWCFG_WAITED_REQ发送的长度
    pthread_mutex_t pMutex;
}stWaitedList;


stWaitedList *waitedList_create(void);
void waitedList_free(stWaitedList *pstWaitedList);
void waitedList_clean(stWaitedList *pstWaitedList);

int waitedList_push(stWaitedList *pstWaitedList, void *pData, int iDataLen);
int waitedList_findAndDelete(stWaitedList *pstWaitedList, unsigned int uiTargetDataID);
int waitedList_traverseAndPack(stWaitedList *pstWaitedList, MSG_NEWCFG_WAITED_REQ *req, int *piActualLen);//调用此接口前需要申请好MSG_NEWCFG_WAITED_REQ的内存

#endif //_LIST_H_
