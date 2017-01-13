#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include <netinet/in.h> //for htons
#include "log.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "waitedList.h"

static unsigned int g_uiWaitedID = 0;

stWaitedList *waitedList_create(void)
{
    stWaitedList *pstWaitedList = (stWaitedList *)malloc(sizeof(stWaitedList));
    if(pstWaitedList != NULL)
    {
        pstWaitedList->pFront = NULL;
        pstWaitedList->pRear = NULL;
        pstWaitedList->uiListSize = 0;
        pstWaitedList->uiMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
        pthread_mutex_init(&pstWaitedList->pMutex, NULL);
    }

    return pstWaitedList; 
}

void waitedList_free(stWaitedList *pstWaitedList)
{
    if(pstWaitedList != NULL)
    {
        waitedList_clean(pstWaitedList);
        pthread_mutex_destroy(&pstWaitedList->pMutex);  
        free(pstWaitedList);  
        pstWaitedList = NULL;
    }

    return;
}

void waitedList_clean(stWaitedList *pstWaitedList)
{
    if(pstWaitedList->uiListSize == 0)
    {
        return;
    }

    stWaitedNode *pNode = pstWaitedList->pFront;
    pthread_mutex_lock(&pstWaitedList->pMutex);
    while(pNode->pNext != NULL) 
    {
        pNode = pNode->pNext;
        free(pNode->pPrev);
        pNode->pPrev = NULL;
    }
    free(pNode);
    pNode = NULL;
    pthread_mutex_unlock(&pstWaitedList->pMutex);

    return;
}

int waitedList_push(stWaitedList *pstWaitedList, void *pData, int iDataLen)
{
    stWaitedNode *pNode = (stWaitedNode *)malloc(sizeof(stWaitedNode));
    if(pNode == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&pstWaitedList->pMutex);
    pNode->cSendTimers = 0;
    pNode->uiWaitedID = ++g_uiWaitedID; //每个配置的ID
    pNode->iDataLen = iDataLen;
    memcpy(pNode->pData, pData, iDataLen);
    pNode->pPrev = pstWaitedList->pRear;
    pNode->pNext = NULL;

    if(pstWaitedList->uiListSize == 0)
    {
        pstWaitedList->pFront = pNode;
    }
    else
    {
        pstWaitedList->pRear->pNext = pNode;
    }
    pstWaitedList->pRear = pNode;
    pstWaitedList->uiListSize++;
    pstWaitedList->uiMsgLen += (sizeof(DATA_NEWCFG) + iDataLen);
    pthread_mutex_unlock(&pstWaitedList->pMutex);

    return 0;
}

int __waitedList_delete(stWaitedList *pstWaitedList, stWaitedNode *pNode)
{
    pstWaitedList->uiListSize--;
    pstWaitedList->uiMsgLen -= (sizeof(DATA_NEWCFG) + pNode->iDataLen);
    if(pstWaitedList->uiListSize == 0)
    {
        pstWaitedList->pFront = NULL;
        pstWaitedList->pRear = NULL;
    }
    else
    {
        if(pNode == pstWaitedList->pFront)
        {
            pstWaitedList->pFront = pNode->pNext;
        }

        if(pNode == pstWaitedList->pRear)
        {
            pstWaitedList->pRear = pNode->pPrev;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);

    return 0;
}

int waitedList_findAndDelete(stWaitedList *pstWaitedList, unsigned int uiTargetDataID)
{
    stWaitedNode *pNode = pstWaitedList->pFront;
    pthread_mutex_lock(&pstWaitedList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->uiWaitedID < uiTargetDataID)
        {
            //turn to the next one
            pNode = pNode->pNext;
        }
        else if(pNode->uiWaitedID == uiTargetDataID)
        {
            //found
            __waitedList_delete(pstWaitedList, pNode);

            log_info("waitedList_findAndDelete uiTargetDataID(%u) ok.", uiTargetDataID);
            return 0;
        }
        else if(pNode->uiWaitedID > uiTargetDataID)
        {
            //error
            log_warning("waitedList_findAndDelete uiTargetDataID(%u) failed!", uiTargetDataID);
            return -1;
        }
    }
    pthread_mutex_unlock(&pstWaitedList->pMutex);

    return -1;
}

//调用此接口前需要申请好MSG_NEWCFG_WAITED_REQ的内存
int waitedList_traverseAndPack(stWaitedList *pstWaitedList, MSG_NEWCFG_WAITED_REQ *req)
{
    if(pstWaitedList->uiListSize == 0)
    {
        return -1;
    }

    DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);
    stWaitedNode *pNode = pstWaitedList->pFront;

    pthread_mutex_lock(&pstWaitedList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->cSendTimers >= 3)
        {
            __waitedList_delete(pstWaitedList, pNode);

            pNode = pNode->pNext;
            continue;
        }

        pDataNewcfg->uiWaitedID = htonl(pNode->uiWaitedID);
        pDataNewcfg->sChecksum = htons(checksum((const char *)pNode->pData, pNode->iDataLen));
        pDataNewcfg->iDataLen = htonl(pNode->iDataLen);
        memcpy(pDataNewcfg->acData, pNode->pData, pNode->iDataLen);

        pDataNewcfg = (DATA_NEWCFG *)((char *)pDataNewcfg + pNode->iDataLen + sizeof(DATA_NEWCFG));//偏移指针以填充下一个配置包
        pNode = pNode->pNext;
    }
    pthread_mutex_unlock(&pstWaitedList->pMutex);

    return 0;
}




