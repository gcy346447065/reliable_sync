#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include <netinet/in.h> //for htons
#include <stdio.h>
#include "log.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "waitedList.h"
#include "macro.h"

static unsigned int g_uiWaitedID = 0;
static stWaitedList *g_pstWaitedList;

int waitedList_init(void)
{
    g_pstWaitedList = (stWaitedList *)malloc(sizeof(stWaitedList));
    if(g_pstWaitedList != NULL)
    {
        g_pstWaitedList->pFront = NULL;
        g_pstWaitedList->pRear = NULL;
        g_pstWaitedList->uiListSize = 0;
        g_pstWaitedList->uiMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
        pthread_mutex_init(&g_pstWaitedList->pMutex, NULL);
    }

    return 0;
}

void waitedList_free(void)
{
    if(g_pstWaitedList != NULL)
    {
        waitedList_clean();
        pthread_mutex_destroy(&g_pstWaitedList->pMutex);
        free(g_pstWaitedList);
        g_pstWaitedList = NULL;
    }

    return;
}

void waitedList_clean(void)
{
    if(g_pstWaitedList->uiListSize == 0)
    {
        return;
    }

    stWaitedNode *pNode = g_pstWaitedList->pFront;
    pthread_mutex_lock(&g_pstWaitedList->pMutex);
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
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    return;
}

int waitedList_push(void *pData, int iDataLen)
{
    stWaitedNode *pNode = (stWaitedNode *)malloc(sizeof(stWaitedNode));
    if(pNode == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    pNode->cSendTimers = 0;
    pNode->uiWaitedID = g_uiWaitedID; //每个配置的ID
    pNode->iDataLen = iDataLen;
    pNode->pData = malloc(iDataLen);
    memcpy(pNode->pData, pData, iDataLen);
    pNode->pPrev = g_pstWaitedList->pRear;
    pNode->pNext = NULL;

    if(g_pstWaitedList->uiListSize == 0)
    {
        g_pstWaitedList->pFront = pNode;
    }
    else
    {
        g_pstWaitedList->pRear->pNext = pNode;
    }
    g_pstWaitedList->pRear = pNode;
    g_pstWaitedList->uiListSize++;
    g_pstWaitedList->uiMsgLen += (sizeof(DATA_NEWCFG) + iDataLen);
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    return 0;
}

int waitedList_ID(int num)          //将g_uiWaitedID拆成两半，前半用作组序号，后半用作每组的序号
{
    static short next_ID[2] = {0};
    g_uiWaitedID = *(unsigned int *)next_ID;printf("g_uiWaitedID:%u\n", g_uiWaitedID);
    if(num)
    {
        next_ID[1]++;
    }
    else
    {
        next_ID[0]++;
        next_ID[1] = 0;
    }
    return 0;
}

int __waitedList_delete(stWaitedNode *pNode, int n)
{
    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    if (g_pstWaitedList->uiListSize > n)
    {
        g_pstWaitedList->uiListSize -= n;
    }
    else
    {
        n = g_pstWaitedList->uiListSize;
        g_pstWaitedList->uiListSize = 0;
    }

    int i;
    stWaitedNode *bNode = pNode, *cNode;
    unsigned int templen = 0;
    pNode = pNode->pPrev;
    for(i=0;i<n && bNode;i++)
    {
        templen += sizeof(DATA_NEWCFG) + bNode->iDataLen;
        free(bNode->pData);
        cNode = bNode;
        bNode = bNode->pNext;
        free(cNode);
    }
    if(pNode == NULL)
    {
        g_pstWaitedList->pFront = bNode;
    }
    else
    {
        pNode->pNext = bNode;
    }
    if(bNode == NULL)
    {
        g_pstWaitedList->pRear = pNode;bNode;
    }
    else
    {
        bNode->pPrev = pNode;
        bNode->pPrev = pNode;bNode;
    }

    g_pstWaitedList->uiMsgLen -= templen;
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    return 0;
}

int waitedList_findAndDelete(unsigned int uiTargetDataID, int n)
{
    stWaitedNode *pNode = g_pstWaitedList->pFront;
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
            __waitedList_delete(pNode, n);

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

    return 0;
}

unsigned int waitedList_getListSize(void)
{
    return g_pstWaitedList->uiListSize;
}

unsigned int waitedList_getMsgLen(void)
{
    return g_pstWaitedList->uiMsgLen;
}

unsigned int waitedList_getadeMsgLen(void)
{
    unsigned int adeMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
    stWaitedNode *pNode = g_pstWaitedList->pFront;
    while (pNode && adeMsgLen < MAX_PKG_LEN)
    {
        adeMsgLen += sizeof(DATA_NEWCFG) + pNode->iDataLen;
        pNode = pNode->pNext;
    }
    return adeMsgLen;
}

stWaitedNode *waitedList_getFrontNode(void)
{
    return g_pstWaitedList->pFront;
}

stWaitedNode *waitedList_getRearNode(void)
{
    return g_pstWaitedList->pRear;
}

//调用此接口前需要申请好MSG_NEWCFG_WAITED_REQ的内存
int waitedList_traverseAndPack(MSG_NEWCFG_WAITED_REQ *req)
{
    if(g_pstWaitedList->uiListSize == 0)
    {
        return -1;
    }
    unsigned int adeMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
    int nodenum = 0;
    DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);
    stWaitedNode *pNode = g_pstWaitedList->pFront;

    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    while(pNode && adeMsgLen < MAX_PKG_LEN)
    {
        if(pNode->cSendTimers >= 3)
        {
            __waitedList_delete(pNode, 1);
            pNode = pNode->pNext;
            continue;
        }
        nodenum++;
        adeMsgLen += sizeof(DATA_NEWCFG) + pNode->iDataLen;
        pDataNewcfg->uiWaitedID = htonl(pNode->uiWaitedID);
        pDataNewcfg->sChecksum = htons(checksum((const char *)pNode->pData, pNode->iDataLen));
        pDataNewcfg->iDataLen = htonl(pNode->iDataLen);
        memcpy(pDataNewcfg->acData, pNode->pData, pNode->iDataLen);

        pDataNewcfg = (DATA_NEWCFG *)((char *)pDataNewcfg + pNode->iDataLen + sizeof(DATA_NEWCFG));//偏移指针以填充下一个配置包
        pNode->cSendTimers++;
        pNode = pNode->pNext;
    }
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    req->sChecksum = htons(checksum((const char *)req->dataNewcfg, adeMsgLen - sizeof(MSG_NEWCFG_WAITED_REQ)));
    req->uiWaitedSum = htonl(nodenum);

    return 0;
}




