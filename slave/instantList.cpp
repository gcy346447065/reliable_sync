#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include "log.h"
#include "event.h"
#include "send.h"
#include "protocol.h"
#include "instantList.h"

extern int g_iSyncSockFd;

static unsigned int g_uiInstantID = 0;
static stInstantList *g_pstInstantList;

int instantList_init(void)
{
    g_pstInstantList = (stInstantList *)malloc(sizeof(stInstantList));
    if(g_pstInstantList != NULL)
    {
        g_pstInstantList->pFront = NULL;
        g_pstInstantList->pNew = NULL;
        g_pstInstantList->pRear = NULL;
        g_pstInstantList->uiListSize = 0;
        g_pstInstantList->uiNewSize = 0;
        pthread_mutex_init(&g_pstInstantList->pMutex, NULL);
    }

    return 0; 
}

void instantList_free(void)
{
    if(g_pstInstantList != NULL)
    {
        instantList_clean();
        pthread_mutex_destroy(&g_pstInstantList->pMutex);  
        free(g_pstInstantList);
        g_pstInstantList = NULL;
    }

    return;
}

void instantList_clean(void)
{
    if(g_pstInstantList->uiListSize == 0)
    {
        return;
    }

    stInstantNode *pNode = g_pstInstantList->pFront;
    pthread_mutex_lock(&g_pstInstantList->pMutex);
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
    pthread_mutex_unlock(&g_pstInstantList->pMutex);

    return;
}

int instantList_push(void *pData, int iDataLen)
{
    stInstantNode *pNode = (stInstantNode *)malloc(sizeof(stInstantNode));
    if(pNode == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_pstInstantList->pMutex);
    pNode->cFindTimers = 0;
    pNode->cSendTimers = 0;
    pNode->uiInstantID = ++g_uiInstantID; //每个配置的ID
    pNode->iDataLen = iDataLen;
    pNode->pData = malloc(iDataLen);
    memcpy(pNode->pData, pData, iDataLen);
    pNode->pPrev = g_pstInstantList->pRear;
    pNode->pNext = NULL;

    if(g_pstInstantList->uiListSize == 0)
    {
        g_pstInstantList->pFront = pNode;
        g_pstInstantList->pNew = pNode;
    }
    else
    {
        g_pstInstantList->pRear->pNext = pNode;
    }
    g_pstInstantList->pRear = pNode;
    g_pstInstantList->uiListSize++;
    g_pstInstantList->uiNewSize++;
    pthread_mutex_unlock(&g_pstInstantList->pMutex);

    return 0;
}

//调用此接口需要保证pNode一定在pstInstantList中！
int instantList_delete(stInstantNode *pNode)
{
    if(g_pstInstantList->uiListSize == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&g_pstInstantList->pMutex);
    g_pstInstantList->uiListSize--;
    if(g_pstInstantList->uiListSize == 0)
    {
        g_pstInstantList->pFront = NULL;
        g_pstInstantList->pRear = NULL;
        g_pstInstantList->pNew = NULL;
    }
    else
    {
        if(pNode == g_pstInstantList->pFront)
        {
            g_pstInstantList->pFront = pNode->pNext;
        }

        if(pNode == g_pstInstantList->pRear)
        {
            g_pstInstantList->pRear = pNode->pPrev;
        }

        if(pNode == g_pstInstantList->pNew)//实际上不可能是pNew，因为删除的肯定是读取过的
        {
            g_pstInstantList->pNew = pNode->pNext;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);
    pthread_mutex_unlock(&g_pstInstantList->pMutex);

    return 0;
}

int instantList_moveNew(void)
{
    if(g_pstInstantList->uiNewSize == 0)
    {
        return -1;
    }

    pthread_mutex_lock(&g_pstInstantList->pMutex);
    g_pstInstantList->uiNewSize--;
    g_pstInstantList->pNew = g_pstInstantList->pNew->pNext;
    pthread_mutex_unlock(&g_pstInstantList->pMutex);

    return 0;
}

stInstantNode *instantList_find(unsigned int uiTargetDataID)
{
    stInstantNode *pNode = g_pstInstantList->pFront;
    pthread_mutex_lock(&g_pstInstantList->pMutex);
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
    pthread_mutex_unlock(&g_pstInstantList->pMutex);

    return pNode;
}

unsigned int instantList_getNewSize(void)
{
    return g_pstInstantList->uiNewSize;
}

unsigned int instantList_getListSize(void)
{
    return g_pstInstantList->uiListSize;
}

stInstantNode *instantList_getFrontNode(void)
{
    return g_pstInstantList->pFront;
}

stInstantNode *instantList_getNewNode(void)
{
    return g_pstInstantList->pNew;
}

stInstantNode *instantList_getRearNode(void)
{
    return g_pstInstantList->pRear;
}



