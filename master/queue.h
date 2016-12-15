#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <pthread.h>

typedef struct Node
{
    void *pData;
    int iDataLen;
    struct Node *pNext;
}stNode;

typedef struct Queue
{
    stNode *pFront;
    stNode *pRear;
    int iSize;
    pthread_mutex_t pMutex;
}stQueue;


stQueue *queue_init();
stNode *queue_push(stQueue *pstQueue, void *pData, int iDataLen);
stNode *queue_pop(stQueue *pstQueue);
void queue_free(stQueue *pstQueue);

void queue_clean(stQueue *pstQueue);
int queue_isEmpty(stQueue *pstQueue);
int queue_getSize(stQueue *pstQueue);

#endif //_QUEUE_H_