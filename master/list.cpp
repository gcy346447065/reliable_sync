#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include "log.h"
#include "event.h"
#include "list.h"

static int g_iDataID = 0;

stList *list_init()
{
    stList *pstList = (stList *)malloc(sizeof(stList));
    if(pstList != NULL)
    {
        pstList->pFront = NULL;
        pstList->pRear = NULL;
        pstList->pReadNow = NULL;
        pstList->iSize = 0;
        pstList->iReadSize = 0;
        pthread_mutex_init(&pstList->pMutex, NULL);
    }

    return pstList; 
}

int list_push(stList *pstList, void *pData, int iDataLen)
{
    int iRet = 0;

    stNode *pNode = (stNode *)malloc(sizeof(stNode));
    if(pNode != NULL)
    {
        pthread_mutex_lock(&pstList->pMutex);
        memcpy(pNode->pData, pData, iDataLen);
        pNode->iDataLen = iDataLen;
        pNode->iDataID = ++g_iDataID;//每个配置的ID
        pNode->iFindTimers = 0;
        pNode->iSendTimers = 0;
        pNode->pPrev = pstList->pRear;
        pNode->pNext = NULL;

        if(list_isEmpty(pstList))
        {
            pstList->pFront = pNode;
            pstList->pReadNow = pNode;
        }
        else
        {
            pstList->pRear->pNext = pNode;
        }
        pstList->pRear = pNode;
        pstList->iSize++;
        pstList->iReadSize++;
        pthread_mutex_unlock(&pstList->pMutex);
    }
    else
    {
        iRet = -1;
    }

    return iRet;
}

int list_read(stList *pstList)
{
    int iRet = 0;
    stNode *pNode = pstList->pReadNow;
    pthread_mutex_lock(&pstList->pMutex);
    if(pstList->iReadSize != 0)
    {
        pstList->iReadSize--;
        pstList->pReadNow = pNode->pNext;
    }
    else
    {
        iRet = -1;
    }
    pthread_mutex_unlock(&pstList->pMutex);

    return iRet;
}

int list_deleteByDataID(stList *pstList, int iTargetDataID)
{
    int iRet = -1;
    stNode *pNode = pstList->pFront;
    pthread_mutex_lock(&pstList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->iDataID == iTargetDataID)
        {
            //found
            pstList->iSize--;
            if(list_isEmpty(pstList))
            {
                pstList->pFront = NULL;
                pstList->pRear = NULL;
                pstList->pReadNow = NULL;
                pstList->iSize = 0;
                pstList->iReadSize = 0;
            }
            else
            {
                if(pNode == pstList->pFront)
                {
                    pstList->pFront = pNode->pNext;
                }

                if(pNode == pstList->pRear)
                {
                    pstList->pRear = pNode->pPrev;
                }

                if(pNode == pstList->pReadNow)//实际上不可能是pReadNow，因为删除的肯定是读取过的
                {
                    pstList->pReadNow = pNode->pNext;
                }
            }

            if(pNode->pPrev != NULL && pNode->pNext != NULL)
            {
                pNode->pPrev->pNext = pNode->pNext;
                pNode->pNext->pPrev = pNode->pPrev;
            }
            free(pNode);

            iRet = 0;//只有找到才能返回0
            log_info("list_deleteByDataID iTargetDataID(%d) ok.", iTargetDataID);
            break;
        }
        else if(pNode->iDataID > iTargetDataID)
        {
            //error
            log_info("list_deleteByDataID iTargetDataID(%d) failed!", iTargetDataID);
            break;
        }
        else if(pNode->iDataID < iTargetDataID)
        {
            //turn to the next one
            pNode = pNode->pNext;
        }
    }
    pthread_mutex_unlock(&pstList->pMutex);

    return iRet;
}

int list_deleteByNode(stList *pstList, stNode *pNode)
{
    pthread_mutex_lock(&pstList->pMutex);

    log_info("list_deleteByNode pNode->iDataID(%d) ok.", pNode->iDataID);

    pstList->iSize--;
    if(list_isEmpty(pstList))
    {
        pstList->pFront = NULL;
        pstList->pRear = NULL;
        pstList->pReadNow = NULL;
        pstList->iSize = 0;
        pstList->iReadSize = 0;
    }
    else
    {
        if(pNode == pstList->pFront)
        {
            pstList->pFront = pNode->pNext;
        }

        if(pNode == pstList->pRear)
        {
            pstList->pRear = pNode->pPrev;
        }

        if(pNode == pstList->pReadNow)//实际上不可能是pReadNow，因为删除的肯定是读取过的
        {
            pstList->pReadNow = pNode->pNext;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);

    pthread_mutex_unlock(&pstList->pMutex);

    return 0;
}

stNode *list_find(stList *pstList, int iTargetDataID)
{
    stNode *pNode = pstList->pFront;
    pthread_mutex_lock(&pstList->pMutex);
    while(pNode != NULL)
    {
        if(pNode->iDataID == iTargetDataID)
        {
            //found
            log_info("list_find iTargetDataID(%d) ok.", iTargetDataID);
            pNode->iFindTimers++;//累加查找次数
            break;
        }
        else if(pNode->iDataID > iTargetDataID)
        {
            //error
            pNode = NULL;
            log_info("list_find iTargetDataID(%d) failed!", iTargetDataID);
            break;
        }
        else if(pNode->iDataID < iTargetDataID)
        {
            //turn to the next one
            pNode->iFindTimers++;//累加查找次数
            pNode = pNode->pNext;
        }
    }
    pthread_mutex_unlock(&pstList->pMutex);

    return pNode;
}

void list_free(stList *pstList)
{
    if(pstList != NULL)
    {
        list_clean(pstList);
        pthread_mutex_destroy(&pstList->pMutex);  
        free(pstList);  
        pstList = NULL;
    }

    return;
}

void list_clean(stList *pstList)
{
    stNode *pNode = pstList->pFront;
    pthread_mutex_lock(&pstList->pMutex);
    while(pNode->pNext != NULL) 
    {
        pNode = pNode->pNext;
        free(pNode->pPrev);
        pNode->pPrev = NULL;
    }
    free(pNode);
    pNode = NULL;
    pthread_mutex_unlock(&pstList->pMutex);

    return;
}

int list_isEmpty(stList *pstList)
{
    if(pstList->pFront==NULL && pstList->pRear==NULL && pstList->iSize==0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int list_getSize(stList *pstList)
{
    return pstList->iSize;
}

int list_getReadSize(stList *pstList)
{
    return pstList->iReadSize;
}



