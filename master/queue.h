#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

typedef struct Node
{
    void *pData;
    int iDataLen;
    int iDataID;
    struct Node *pNext;
}stNode;

typedef struct Queue
{
    stNode *pFront;
    stNode *pReadNow;
    stNode *pRear;
    int iSize;
    int iReadSize;
    pthread_mutex_t pMutex;
}stQueue;


stQueue *queue_init();
stNode *queue_push(stQueue *pstQueue, void *pData, int iDataLen);
stNode *queue_pop(stQueue *pstQueue);
stNode *queue_read(stQueue *pstQueue);
stNode *queue_foreach(stQueue *pstQueue, int iTargetDataID);
void queue_free(stQueue *pstQueue);

void queue_clean(stQueue *pstQueue);
int queue_isEmpty(stQueue *pstQueue);
int queue_getSize(stQueue *pstQueue);
int queue_getReadSize(stQueue *pstQueue);

#endif //_QUEUE_H_