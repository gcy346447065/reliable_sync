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

int waitedList_ID(int num)          //将g_uiWaitedID拆成四，一用作组序号，二用作每组的序号，三用作结束标志，四在出队列时填写
{
    static char next_ID[4] = {0};
    g_uiWaitedID = *(unsigned int *)next_ID;
    if(num == 1)
    {
        next_ID[1]++;
    }
    else
    {
        ((char *)&g_uiWaitedID)[2] = 1;
        next_ID[0]++;
        next_ID[1] = 0;
    }
    return 0;
}

int __waitedList_delete(stWaitedNode *pNode, int n)
{printf("get into del listsize:%d\n", g_pstWaitedList->uiListSize);
    //pthread_mutex_lock(&g_pstWaitedList->pMutex);
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
    //pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    return 0;
}

int __waited_Add(stWaitedNode *pNode, void *pData, int iDataLen)
{
    //pthread_mutex_lock(&g_pstWaitedList->pMutex);printf("stay\n");
    stWaitedNode *nNode = (stWaitedNode *)malloc(sizeof(stWaitedNode));
    if(nNode == NULL)
    {
        return -1;
    }
    if(pNode == NULL)
    {
        return -1;
    }
    nNode->cSendTimers = 0;
    nNode->uiWaitedID = pNode->uiWaitedID + 1; //每个配置的ID
    nNode->iDataLen = iDataLen;
    nNode->pData = malloc(iDataLen);
    memcpy(nNode->pData, pData, iDataLen);
    nNode->pPrev = pNode;
    nNode->pNext = pNode->pNext;
    pNode->pNext = nNode;
    if(g_pstWaitedList->pRear == pNode)
    {
        g_pstWaitedList->pRear = nNode;
    }
    g_pstWaitedList->uiListSize++;
    g_pstWaitedList->uiMsgLen += (sizeof(DATA_NEWCFG) + iDataLen);
    //pthread_mutex_unlock(&g_pstWaitedList->pMutex);printf("add4\n");

    return 0;
}

int waitedList_findAndDelete(unsigned int uiTargetDataID, int n)
{printf("waited find del uiID:%d, n:%d, list_size:%d\n", uiTargetDataID, n, g_pstWaitedList->uiListSize);
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
            __waitedList_delete(pNode, n);printf("waited find del list_size:%d\n", g_pstWaitedList->uiListSize);

            log_info("waitedList_findAndDelete uiTargetDataID(%u) ok.", uiTargetDataID);
            return 0;
        }
        else if(pNode->uiWaitedID > uiTargetDataID)
        {
            //error
            log_warning("waitedList_findAndDelete uiTargetDataID(%u) failed!", uiTargetDataID);
            return -1;
        }
    }printf("waited find del list_size:%d\n", g_pstWaitedList->uiListSize);

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
int waitedList_traverseAndPack(int *lenth, MSG_NEWCFG_WAITED_REQ *req)
{printf("get into tra pack\n");
    if(g_pstWaitedList->uiListSize == 0)
    {
        *lenth = 0;
        return -1;
    }
    unsigned int adeMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
    int nodenum = 0, cut_len, eol;
    DATA_NEWCFG *pDataNewcfg = (DATA_NEWCFG *)(req->dataNewcfg);
    stWaitedNode *pNode;
    static stWaitedNode *pnextNode;
    if(pnextNode == NULL)
    {
        pNode = g_pstWaitedList->pFront;
    }
    else
    {
        pNode = pnextNode;
    }
    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    while(pNode && adeMsgLen < MAX_PKG_LEN )
    {
        if(pNode->cSendTimers >= 3)
        {
            __waitedList_delete(pNode, 1);
            pNode = pNode->pNext;
            continue;
        }
        nodenum++;
        if(adeMsgLen + sizeof(DATA_NEWCFG) + pNode->iDataLen > MAX_BUFFER_SIZE)
        {
            if (nodenum == 1)
            {
                cut_len = MAX_BUFFER_SIZE - adeMsgLen - sizeof(DATA_NEWCFG);
                __waited_Add(pNode, pNode->pData + cut_len, pNode->iDataLen - cut_len);
                pNode->iDataLen = cut_len;
            }
            else
            {
                break;
            }
        }
        adeMsgLen += sizeof(DATA_NEWCFG) + pNode->iDataLen;
        pDataNewcfg->uiWaitedID = htonl(pNode->uiWaitedID);
        pDataNewcfg->sChecksum = htons(checksum((const char *)pNode->pData, pNode->iDataLen));
        pDataNewcfg->iDataLen = htonl(pNode->iDataLen);
        memcpy(pDataNewcfg->acData, pNode->pData, pNode->iDataLen);

        pDataNewcfg = (DATA_NEWCFG *)((char *)pDataNewcfg + pNode->iDataLen + sizeof(DATA_NEWCFG));
        pNode->cSendTimers++;
        pNode = pNode->pNext;
    }
    pnextNode = pNode;
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    req->sChecksum = htons(checksum((const char *)req->dataNewcfg, adeMsgLen - sizeof(MSG_NEWCFG_WAITED_REQ)));
    req->uiWaitedSum = htonl(nodenum);

    *lenth = int((char *)pDataNewcfg - (char *)req);
    req->msgHeader.iLength = htonl(*lenth - MSG_HEADER_LEN);
    eol = pnextNode != NULL;
}




