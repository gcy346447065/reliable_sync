#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include <netinet/in.h> //for htons
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "log.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "waitedList.h"

#define MAX_STDIN_FILE_LEN 128

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

int waitedList_push(void *pData, int iDataLen, unsigned int uiWaitedID)
{
    stWaitedNode *pNode = (stWaitedNode *)malloc(sizeof(stWaitedNode));
    if(pNode == NULL)
    {
        return -1;
    }

    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    pNode->cSendTimers = 0;
    pNode->uiWaitedID = uiWaitedID; //每个配置的ID
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

int __waitedList_delete(stWaitedNode *pNode)
{
    pthread_mutex_lock(&g_pstWaitedList->pMutex);
    g_pstWaitedList->uiListSize--;
    g_pstWaitedList->uiMsgLen -= (sizeof(DATA_NEWCFG) + pNode->iDataLen);
    if(g_pstWaitedList->uiListSize == 0)
    {
        g_pstWaitedList->pFront = NULL;
        g_pstWaitedList->pRear = NULL;
    }
    else
    {
        if(pNode == g_pstWaitedList->pFront)
        {
            g_pstWaitedList->pFront = pNode->pNext;
        }

        if(pNode == g_pstWaitedList->pRear)
        {
            g_pstWaitedList->pRear = pNode->pPrev;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);
    pthread_mutex_unlock(&g_pstWaitedList->pMutex);

    return 0;
}

int waitedList_file()
{
    stWaitedNode *pNode = g_pstWaitedList->pFront;
    int i, fd, group;
    char *pcFilenameWaited = (char *)malloc(MAX_STDIN_FILE_LEN);
    while(pNode)
    {
        memset(pcFilenameWaited, 0, MAX_STDIN_FILE_LEN);
        group = ((char *)&pNode->uiWaitedID)[0];
        sprintf(pcFilenameWaited, "file%d", group+1);
        if ((fd = open(pcFilenameWaited, O_RDWR|O_CREAT|O_APPEND, 00700)) == -1)
        {
            log_error("open %s wrong\n", pcFilenameWaited);
            return -1;
        }
        do
        {
            log_debug("write:%d\n", write(fd, pNode->pData, pNode->iDataLen));
            pNode = pNode->pNext;
            __waitedList_delete(g_pstWaitedList->pFront);
        }while(pNode && ((char *)&pNode->uiWaitedID)[0] == group);
        close(fd);
    }
    free(pcFilenameWaited);
    return 0;
}
int waitedList_findAndDelete(unsigned int uiTargetDataID)
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
            __waitedList_delete(pNode);

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

stWaitedNode *waitedList_getFrontNode(void)
{
    return g_pstWaitedList->pFront;
}

stWaitedNode *waitedList_getRearNode(void)
{
    return g_pstWaitedList->pRear;
}




