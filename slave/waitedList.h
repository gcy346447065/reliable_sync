#ifndef _WAITED_LIST_H_
#define _WAITED_LIST_H_

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


int waitedList_init(void);
void waitedList_free(void);
void waitedList_clean(void);

int waitedList_push(void *pData, int iDataLen, unsigned int uiWaitedID);
int waitedList_findAndDelete(unsigned int uiTargetDataID);
int waitedList_file();

unsigned int waitedList_getListSize(void);
unsigned int waitedList_getMsgLen(void);
stWaitedNode *waitedList_getFrontNode(void);
stWaitedNode *waitedList_getRearNode(void);

#endif //_WAITED_LIST_H_
