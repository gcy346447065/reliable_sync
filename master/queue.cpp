#include <stdlib.h> //for malloc
#include "queue.h"

stQueue *queue_init()
{
    stQueue *pstQueue = (stQueue *)malloc(sizeof(stQueue));
    if(pstQueue != NULL)
    {
        pstQueue->pFront = NULL;
        pstQueue->pRear = NULL;
        pstQueue->iSize = 0;
        pthread_mutex_init(&pstQueue->pMutex, NULL);
    }

    return pstQueue; 
}

stNode *queue_push(stQueue *pstQueue, void *pData, int iDataLen)
{
    stNode *pNode = (stNode *)malloc(sizeof(stNode));
    if(pNode != NULL)
    {
        pNode->pData = pData;
        pNode->iDataLen = iDataLen;
        pNode->pNext = NULL;

        pthread_mutex_lock(&pstQueue->pMutex);
        if(queue_isEmpty(pstQueue))
        {
            pstQueue->pFront = pNode;
        }
        else
        {
            pstQueue->pRear->pNext = pNode;
        }
        pstQueue->pRear = pNode;
        pstQueue->iSize++;
        pthread_mutex_unlock(&pstQueue->pMutex);
    }

    return pNode;
}

stNode *queue_pop(stQueue *pstQueue)
{
    stNode *pNode = pstQueue->pFront;
    pthread_mutex_lock(&pstQueue->pMutex);
    if(!queue_isEmpty(pstQueue))
    {
        pstQueue->iSize--;
        pstQueue->pFront = pNode->pNext;
        free(pNode);
        if(pstQueue->iSize == 0)
        {
            pstQueue->pRear = NULL;
        }
    }
    pthread_mutex_unlock(&pstQueue->pMutex);

    return pstQueue->pFront;
}

void queue_free(stQueue *pstQueue)
{
    if(pstQueue != NULL)
    {
        queue_clean(pstQueue);
        pthread_mutex_destroy(&pstQueue->pMutex);  
        free(pstQueue);  
        pstQueue = NULL;
    }

    return;
}

void queue_clean(stQueue *pstQueue)
{
    while(!queue_isEmpty(pstQueue)) 
    {
        queue_pop(pstQueue);
    }

    return;
}

int queue_isEmpty(stQueue *pstQueue)
{
    if(pstQueue->pFront==NULL && pstQueue->pRear==NULL && pstQueue->iSize==0)
    {
        return 1;  
    }
    else
    {
        return 0; 
    }
}

int queue_getSize(stQueue *pstQueue)
{
    return pstQueue->iSize;
}



