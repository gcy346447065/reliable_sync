#include <stdlib.h> //for malloc
#include <netinet/in.h> //for htons
#include <string.h> //for memcpy
#include "list_data.h"
#include "log.h"

static DATA_LIST_S *g_pstDataList;

DWORD list_data::data_init(void)
{
    g_pstDataList = (DATA_LIST_S *)malloc(sizeof(DATA_LIST_S));
    if(g_pstDataList != NULL)
    {
        pthread_mutex_init(&g_pstDataList->pBatchMutex, NULL);
        g_pstDataList->pBatchFront = NULL;
        g_pstDataList->pBatchRear = NULL;
        g_pstDataList->dwBatchCount = 0;
        g_pstDataList->dwBatchNew = 0;

        pthread_mutex_init(&g_pstDataList->pInstantMutex, NULL);
        g_pstDataList->pInstantFront = NULL;
        g_pstDataList->pInstantRear = NULL;
        g_pstDataList->dwInstantCount = 0;
        g_pstDataList->dwInstantNew = 0;

        pthread_mutex_init(&g_pstDataList->pWaitedMutex, NULL);
        g_pstDataList->pWaitedFront = NULL;
        g_pstDataList->pWaitedRear = NULL;
        g_pstDataList->dwWaitedCount = 0;
        g_pstDataList->dwWaitedNew = 0;
    }

    return SUCCESS;
}

DWORD list_data::data_free(void)
{
    if(g_pstDataList != NULL)
    {
        data_clean();
        pthread_mutex_destroy(&g_pstDataList->pBatchMutex);
        pthread_mutex_destroy(&g_pstDataList->pInstantMutex);
        pthread_mutex_destroy(&g_pstDataList->pWaitedMutex);
        free(g_pstDataList);
        g_pstDataList = NULL;
    }

    return SUCCESS;
}

DWORD list_data::data_clean(void)
{
    if(g_pstDataList->dwBatchCount > 0)
    {
        DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
        while(pNode->pNext != NULL)
        {
            pNode = pNode->pNext;
            free(pNode->pPrev);
            pNode->pPrev = NULL;
        }
        free(pNode);
    }

    if(g_pstDataList->dwInstantCount > 0)
    {
        DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
        while(pNode->pNext != NULL)
        {
            pNode = pNode->pNext;
            free(pNode->pPrev);
            pNode->pPrev = NULL;
        }
        free(pNode);
    }

    if(g_pstDataList->dwWaitedCount > 0)
    {
        DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
        while(pNode->pNext != NULL)
        {
            pNode = pNode->pNext;
            free(pNode->pPrev);
            pNode->pPrev = NULL;
        }
        free(pNode);
    }

    return SUCCESS;
}

static DWORD data_copyFromDataNet(MSG_DATA_S *pstDstData, const MSG_DATA_S *pstScrDataNET, BOOL bIsBatch)
{
    pstDstData->wDataSig = ntohs(pstScrDataNET->wDataSig);
    pstDstData->wDataSeq = ntohs(pstScrDataNET->wDataSeq);
    pstDstData->byDataType = pstScrDataNET->byDataType;
    pstDstData->wDataID = ntohs(pstScrDataNET->wDataID);
    if(bIsBatch)//如果不是batch下面两个值就是默认为0
    {
        pstDstData->wBatchStart = ntohs(pstScrDataNET->wBatchStart);
        pstDstData->wBatchEnd = ntohs(pstScrDataNET->wBatchEnd);
    }
    pstDstData->wDataLen = ntohs(pstScrDataNET->wDataLen);//wDataChecksum这里默认为0
    memcpy(pstDstData->abyData, pstScrDataNET->abyData, ntohs(pstScrDataNET->wDataLen));

    return SUCCESS;
}

static DWORD data_insertBatch(const MSG_DATA_S *pstDataNET)
{
    DATA_NODE_S *pNode = (DATA_NODE_S *)malloc(sizeof(DATA_NODE_S) + ntohs(pstDataNET->wDataLen));
    if(pNode == NULL)
    {
        log_error("malloc error!");
        return FAILE;
    }

    pNode->byDataType = pstDataNET->byDataType;
    pNode->wDataID = ntohs(pstDataNET->wDataID);
    data_copyFromDataNet(&(pNode->stData), pstDataNET, TRUE);

    //链表还是空的，节点直接放入
    if(g_pstDataList->dwBatchCount == 0)
    {
        //log_debug("Batch wDataID(%d) should insert in empty list.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pBatchMutex);
        pNode->pPrev = NULL;
        pNode->pNext = NULL;
        g_pstDataList->pBatchFront = pNode;
        g_pstDataList->pBatchRear = pNode;
        g_pstDataList->dwBatchCount++;
        g_pstDataList->dwBatchNew++;
        pthread_mutex_unlock(&g_pstDataList->pBatchMutex);
        return SUCCESS;
    }

    //节点放在链表头前
    if(g_pstDataList->pBatchFront->wDataID > pNode->wDataID)
    {
        //log_debug("Batch wDataID(%d) should insert at list head.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pBatchMutex);
        pNode->pPrev = NULL;
        pNode->pNext = g_pstDataList->pBatchFront;
        g_pstDataList->pBatchFront->pPrev = pNode;
        g_pstDataList->pBatchFront = pNode;
        g_pstDataList->dwBatchCount++;
        g_pstDataList->dwBatchNew++;
        pthread_mutex_unlock(&g_pstDataList->pBatchMutex);
        return SUCCESS;
    }

    //节点放在链表尾后
    if(g_pstDataList->pBatchRear->wDataID < pNode->wDataID)
    {
        //log_debug("Batch wDataID(%d) should insert at list end.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pBatchMutex);
        pNode->pPrev = g_pstDataList->pBatchRear;
        pNode->pNext = NULL;
        g_pstDataList->pBatchRear->pNext = pNode;
        g_pstDataList->pBatchRear = pNode;
        g_pstDataList->dwBatchCount++;
        g_pstDataList->dwBatchNew++;
        pthread_mutex_unlock(&g_pstDataList->pBatchMutex);
        return SUCCESS;
    }

    //节点放在链表中间，或者节点重复不加入
    DATA_NODE_S *pTmpNode = g_pstDataList->pBatchFront;
    while(pTmpNode != NULL)
    {
        if(pTmpNode->wDataID < pNode->wDataID)
        {
            //节点的备机地址比目标地址小，继续查找
            pTmpNode = pTmpNode->pNext;
        }
        else if(pTmpNode->wDataID == pNode->wDataID)
        {
            //节点的备机地址与目标地址相同，说明已经登录过了
            log_error("Batch wDataID(%d) repeated error!", pNode->wDataID);
            free(pNode);
            return FAILE;
        }
        else if(pTmpNode->wDataID > pNode->wDataID)
        {
            //节点的备机地址比目标地址大，说明目标地址可以插在节点前面
            //log_debug("Batch wDataID(%d) should insert at list middle.", pNode->wDataID);
            pthread_mutex_lock(&g_pstDataList->pBatchMutex);
            pNode->pPrev = pTmpNode->pPrev;
            pNode->pNext = pTmpNode;
            pTmpNode->pPrev->pNext = pNode;
            pTmpNode->pPrev = pNode;
            g_pstDataList->dwBatchCount++;
            g_pstDataList->dwBatchNew++;
            pthread_mutex_unlock(&g_pstDataList->pBatchMutex);
            return SUCCESS;
        }
    }

    return SUCCESS;
}

static DWORD data_insertInstant(const MSG_DATA_S *pstDataNET)
{
    DATA_NODE_S *pNode = (DATA_NODE_S *)malloc(sizeof(DATA_NODE_S) + ntohs(pstDataNET->wDataLen));
    if(pNode == NULL)
    {
        log_error("malloc error!");
        return FAILE;
    }

    pNode->byDataType = pstDataNET->byDataType;
    pNode->wDataID = ntohs(pstDataNET->wDataID);
    data_copyFromDataNet(&(pNode->stData), pstDataNET, FALSE);

    //链表还是空的，节点直接放入
    if(g_pstDataList->dwInstantCount == 0)
    {
        //log_debug("Instant wDataID(%d) should insert in empty list.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pInstantMutex);
        pNode->pPrev = NULL;
        pNode->pNext = NULL;
        g_pstDataList->pInstantFront = pNode;
        g_pstDataList->pInstantRear = pNode;
        g_pstDataList->dwInstantCount++;
        g_pstDataList->dwInstantNew++;
        pthread_mutex_unlock(&g_pstDataList->pInstantMutex);
        return SUCCESS;
    }

    //节点放在链表头前
    if(g_pstDataList->pInstantFront->wDataID > pNode->wDataID)
    {
        //log_debug("Instant wDataID(%d) should insert at list head.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pInstantMutex);
        pNode->pPrev = NULL;
        pNode->pNext = g_pstDataList->pInstantFront;
        g_pstDataList->pInstantFront->pPrev = pNode;
        g_pstDataList->pInstantFront = pNode;
        g_pstDataList->dwInstantCount++;
        g_pstDataList->dwInstantNew++;
        pthread_mutex_unlock(&g_pstDataList->pInstantMutex);
        return SUCCESS;
    }

    //节点放在链表尾后
    if(g_pstDataList->pInstantRear->wDataID < pNode->wDataID)
    {
        //log_debug("Instant wDataID(%d) should insert at list end.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pInstantMutex);
        pNode->pPrev = g_pstDataList->pInstantRear;
        pNode->pNext = NULL;
        g_pstDataList->pInstantRear->pNext = pNode;
        g_pstDataList->pInstantRear = pNode;
        g_pstDataList->dwInstantCount++;
        g_pstDataList->dwInstantNew++;
        pthread_mutex_unlock(&g_pstDataList->pInstantMutex);
        return SUCCESS;
    }

    //节点放在链表中间，或者节点重复不加入
    DATA_NODE_S *pTmpNode = g_pstDataList->pInstantFront;
    while(pTmpNode != NULL)
    {
        if(pTmpNode->wDataID < pNode->wDataID)
        {
            //节点的备机地址比目标地址小，继续查找
            pTmpNode = pTmpNode->pNext;
        }
        else if(pTmpNode->wDataID == pNode->wDataID)
        {
            //节点的备机地址与目标地址相同，说明已经登录过了
            log_error("Instant wDataID(%d) repeated error!", pNode->wDataID);
            free(pNode);
            return FAILE;
        }
        else if(pTmpNode->wDataID > pNode->wDataID)
        {
            //节点的备机地址比目标地址大，说明目标地址可以插在节点前面
            //log_debug("Instant wDataID(%d) should insert at list middle.", pNode->wDataID);
            pthread_mutex_lock(&g_pstDataList->pInstantMutex);
            pNode->pPrev = pTmpNode->pPrev;
            pNode->pNext = pTmpNode;
            pTmpNode->pPrev->pNext = pNode;
            pTmpNode->pPrev = pNode;
            g_pstDataList->dwInstantCount++;
            g_pstDataList->dwInstantNew++;
            pthread_mutex_unlock(&g_pstDataList->pInstantMutex);
            return SUCCESS;
        }
    }

    return SUCCESS;
}

static DWORD data_insertWaited(const MSG_DATA_S *pstDataNET)
{
    DATA_NODE_S *pNode = (DATA_NODE_S *)malloc(sizeof(DATA_NODE_S) + ntohs(pstDataNET->wDataLen));
    if(pNode == NULL)
    {
        log_error("malloc error!");
        return FAILE;
    }

    pNode->byDataType = pstDataNET->byDataType;
    pNode->wDataID = ntohs(pstDataNET->wDataID);
    data_copyFromDataNet(&(pNode->stData), pstDataNET, FALSE);

    //链表还是空的，节点直接放入
    if(g_pstDataList->dwWaitedCount == 0)
    {
        //log_debug("Waited wDataID(%d) should insert in empty list.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pWaitedMutex);
        pNode->pPrev = NULL;
        pNode->pNext = NULL;
        g_pstDataList->pWaitedFront = pNode;
        g_pstDataList->pWaitedRear = pNode;
        g_pstDataList->dwWaitedCount++;
        g_pstDataList->dwWaitedNew++;
        pthread_mutex_unlock(&g_pstDataList->pWaitedMutex);
        return SUCCESS;
    }

    //节点放在链表头前
    if(g_pstDataList->pWaitedFront->wDataID > pNode->wDataID)
    {
        //log_debug("Waited wDataID(%d) should insert at list head.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pBatchMutex);
        pNode->pPrev = NULL;
        pNode->pNext = g_pstDataList->pWaitedFront;
        g_pstDataList->pWaitedFront->pPrev = pNode;
        g_pstDataList->pWaitedFront = pNode;
        g_pstDataList->dwWaitedCount++;
        g_pstDataList->dwWaitedNew++;
        pthread_mutex_unlock(&g_pstDataList->pWaitedMutex);
        return SUCCESS;
    }

    //节点放在链表尾后
    if(g_pstDataList->pWaitedRear->wDataID < pNode->wDataID)
    {
        //log_debug("Waited wDataID(%d) should insert at list end.", pNode->wDataID);
        pthread_mutex_lock(&g_pstDataList->pWaitedMutex);
        pNode->pPrev = g_pstDataList->pWaitedRear;
        pNode->pNext = NULL;
        g_pstDataList->pWaitedRear->pNext = pNode;
        g_pstDataList->pWaitedRear = pNode;
        g_pstDataList->dwWaitedCount++;
        g_pstDataList->dwWaitedNew++;
        pthread_mutex_unlock(&g_pstDataList->pWaitedMutex);
        return SUCCESS;
    }

    //节点放在链表中间，或者节点重复不加入
    DATA_NODE_S *pTmpNode = g_pstDataList->pWaitedFront;
    while(pTmpNode != NULL)
    {
        if(pTmpNode->wDataID < pNode->wDataID)
        {
            //节点的备机地址比目标地址小，继续查找
            pTmpNode = pTmpNode->pNext;
        }
        else if(pTmpNode->wDataID == pNode->wDataID)
        {
            //节点的备机地址与目标地址相同，说明已经登录过了
            log_error("Waited wDataID(%d) repeated error!", pNode->wDataID);
            free(pNode);
            return FAILE;
        }
        else if(pTmpNode->wDataID > pNode->wDataID)
        {
            //节点的备机地址比目标地址大，说明目标地址可以插在节点前面
            //log_debug("Waited wDataID(%d) should insert at list middle.", pNode->wDataID);
            pthread_mutex_lock(&g_pstDataList->pWaitedMutex);
            pNode->pPrev = pTmpNode->pPrev;
            pNode->pNext = pTmpNode;
            pTmpNode->pPrev->pNext = pNode;
            pTmpNode->pPrev = pNode;
            g_pstDataList->dwWaitedCount++;
            g_pstDataList->dwWaitedNew++;
            pthread_mutex_unlock(&g_pstDataList->pWaitedMutex);
            return SUCCESS;
        }
    }

    return SUCCESS;
}

DWORD list_data::data_insert(const MSG_DATA_S *pstDataNET)
{
    switch(pstDataNET->byDataType)
    {
        case DATA_TYPE_BATCH:
            data_insertBatch(pstDataNET);
            break;

        case DATA_TYPE_INSTANT:
            data_insertInstant(pstDataNET);
            break;

        case DATA_TYPE_WAITED:
            data_insertWaited(pstDataNET);
            break;

        default:
            log_error("pstDataNET->byDataType(%d) error!", pstDataNET->byDataType);
            break;
    }

    return SUCCESS;
}

DWORD list_data::data_traverseAndPrintBatch(void)
{
    WORD wStartDataID = 0, wEndDataID = 0;
    DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
    while(pNode != NULL)
    {
        if(wStartDataID == 0)
        {
            wStartDataID = pNode->wDataID;
        }
        
        if(pNode->pNext == NULL)
        {
            wEndDataID = pNode->wDataID;
        }
        else if(pNode->pNext->wDataID - pNode->wDataID == 1)
        {
            wEndDataID = pNode->pNext->wDataID;
        }
        else
        {
            log_debug("Batch(%d:%d).", wStartDataID, wEndDataID);
            wStartDataID = 0;
            wEndDataID = 0;
        }

        pNode = pNode->pNext;
    }

    return SUCCESS;
}

DWORD list_data::data_traverseAndPrintInstant(void)
{
    WORD wStartDataID = 0, wEndDataID = 0;
    DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
    while(pNode != NULL)
    {
        if(wStartDataID == 0)
        {
            wStartDataID = pNode->wDataID;
        }
        
        if(pNode->pNext == NULL)
        {
            wEndDataID = pNode->wDataID;
        }
        else if(pNode->pNext->wDataID - pNode->wDataID == 1)
        {
            wEndDataID = pNode->pNext->wDataID;
        }
        else
        {
            log_debug("Batch(%d:%d).", wStartDataID, wEndDataID);
            wStartDataID = 0;
            wEndDataID = 0;
        }

        pNode = pNode->pNext;
    }

    return SUCCESS;
}

DWORD list_data::data_traverseAndPrintWaited(void)
{
    WORD wStartDataID = 0, wEndDataID = 0;
    DATA_NODE_S *pNode = g_pstDataList->pBatchFront;
    while(pNode != NULL)
    {
        if(wStartDataID == 0)
        {
            wStartDataID = pNode->wDataID;
        }
        
        if(pNode->pNext == NULL)
        {
            wEndDataID = pNode->wDataID;
        }
        else if(pNode->pNext->wDataID - pNode->wDataID == 1)
        {
            wEndDataID = pNode->pNext->wDataID;
        }
        else
        {
            log_debug("Batch(%d:%d).", wStartDataID, wEndDataID);
            wStartDataID = 0;
            wEndDataID = 0;
        }

        pNode = pNode->pNext;
    }

    return SUCCESS;
}

DWORD list_data::data_getBatchCount(void)
{
    return g_pstDataList->dwBatchCount;
}

DWORD list_data::data_getBatchNew(void)
{
    return g_pstDataList->dwBatchNew;
}

DWORD list_data::data_getInstantCount(void)
{
    return g_pstDataList->dwInstantCount;
}

DWORD list_data::data_getInstantNew(void)
{
    return g_pstDataList->dwInstantNew;
}

DWORD list_data::data_getWaitedCount(void)
{
    return g_pstDataList->dwWaitedCount;
}

DWORD list_data::data_getWaitedNew(void)
{
    return g_pstDataList->dwWaitedNew;
}
