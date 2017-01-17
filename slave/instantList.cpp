#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include "log.h"
#include "event.h"
#include "send.h"
#include "protocol.h"
#include "instantList.h"

extern int g_iSyncSockFd;

static unsigned int g_uiInstantID = 0;

stInstantList *instantList_create(void)
{
    stInstantList *pstInstantList = (stInstantList *)malloc(sizeof(stInstantList));
    if(pstInstantList != NULL)
    {
        pstInstantList->pFront = NULL;
        pstInstantList->pNew = NULL;
        pstInstantList->pRear = NULL;
        pstInstantList->uiListSize = 0;
        pstInstantList->uiNewSize = 0;
        pthread_mutex_init(&pstInstantList->pMutex, NULL);
    }

    return pstInstantList; 
}

void instantList_free(stInstantList *pstInstantList)
{
    if(pstInstantList != NULL)
    {
        instantList_clean(pstInstantList);
        pthread_mutex_destroy(&pstInstantList->pMutex);  
        free(pstInstantList);
        pstInstantList = NULL;
    }

    return;
}

void instantList_clean(stInstantList *pstInstantList)
{
    if(pstInstantList->uiListSize == 0)
    {
        return;
    }

    stInstantNode *pNode = pstInstantList->pFront;
    pthread_mutex_lock(&pstInstantList->pMutex);
    while(pNode->pNext != NULL)
    {
        pNode = pNode->pNext;
        free(pNode->pPrev->pData);
        free(pNode->pPrev);
        pNode->pPrev = NULL;
    }
    free(pNode->pData);
    free(pNode);
    pNode = NULL;
    pthread_mutex_unlock(&pstInstantList->pMutex);

    return;
}

int instantList_push(stInstantList *pstInstantList, void *pData, int iDataLen)
{
    stInstantNode *pNode = (stInstantNode *)malloc(sizeof(stInstantNode));
    if(pNode == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&pstInstantList->pMutex);
    pNode->cFindTimers = 0;
    pNode->cSendTimers = 0;
    pNode->uiInstantID = ++g_uiInstantID; //每个配置的ID
    pNode->iDataLen = iDataLen;
    pNode->pData = malloc(iDataLen);
    memcpy(pNode->pData, pData, iDataLen);
    pNode->pPrev = pstInstantList->pRear;
    pNode->pNext = NULL;

    if(pstInstantList->uiListSize == 0)
    {
        pstInstantList->pFront = pNode;
        pstInstantList->pNew = pNode;
    }
    else
    {
        pstInstantList->pRear->pNext = pNode;
    }
    pstInstantList->pRear = pNode;
    pstInstantList->uiListSize++;
    pstInstantList->uiNewSize++;
    pthread_mutex_unlock(&pstInstantList->pMutex);

    return 0;
}

//调用此接口需要保证pNode一定在pstInstantList中！
int instantList_delete(stInstantList *pstInstantList, stInstantNode *pNode)
{
    if(pstInstantList->uiListSize == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&pstInstantList->pMutex);
    pstInstantList->uiListSize--;
    if(pstInstantList->uiListSize == 0)
    {
        pstInstantList->pFront = NULL;
        pstInstantList->pRear = NULL;
        pstInstantList->pNew = NULL;
    }
    else
    {
        if(pNode == pstInstantList->pFront)
        {
            pstInstantList->pFront = pNode->pNext;
        }

        if(pNode == pstInstantList->pRear)
        {
            pstInstantList->pRear = pNode->pPrev;
        }

        if(pNode == pstInstantList->pNew)//实际上不可能是pNew，因为删除的肯定是读取过的
        {
            pstInstantList->pNew = pNode->pNext;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);
    pthread_mutex_unlock(&pstInstantList->pMutex);

    return 0;
}

int instantList_moveNew(stInstantList *pstInstantList)
{
    if(pstInstantList->uiNewSize == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&pstInstantList->pMutex);
    pstInstantList->uiNewSize--;
    pstInstantList->pNew = pstInstantList->pNew->pNext;
    pthread_mutex_unlock(&pstInstantList->pMutex);

    return 0;
}

stInstantNode *instantList_find(stInstantList *pstInstantList, unsigned int uiTargetDataID)
{
    stInstantNode *pNode = pstInstantList->pFront;
    pthread_mutex_lock(&pstInstantList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->uiInstantID < uiTargetDataID)
        {
            //turn to the next one
            pNode->cFindTimers++;//累加查找次数
            pNode = pNode->pNext;
        }
        else if(pNode->uiInstantID == uiTargetDataID)
        {
            //found
            log_info("instantList_find uiTargetDataID(%u) ok.", uiTargetDataID);
            break;
        }
        else if(pNode->uiInstantID > uiTargetDataID)
        {
            //error
            pNode = NULL;
            log_info("instantList_find uiTargetDataID(%u) failed!", uiTargetDataID);
            break;
        }
    }
    pthread_mutex_unlock(&pstInstantList->pMutex);

    return pNode;
}





