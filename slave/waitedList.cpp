#include <stdlib.h> //for malloc
#include <string.h> //for memcpy
#include <netinet/in.h> //for htons
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "log.h"
#include "event.h"
#include "checksum.h"
#include "protocol.h"
#include "waitedList.h"



static stWaitedList *g_pstWaitedList[MAX_REMAIN_FILE];

int waitedList_init(void)
{
    int i;
    for(i=0;i<MAX_REMAIN_FILE;i++)
    {
        g_pstWaitedList[i] = (stWaitedList *)malloc(sizeof(stWaitedList));
        if(g_pstWaitedList[i] != NULL)
        {
            g_pstWaitedList[i]->pFront = NULL;
            g_pstWaitedList[i]->pRear = NULL;
            g_pstWaitedList[i]->uiListSize = 0;
            g_pstWaitedList[i]->uiMsgLen = sizeof(MSG_NEWCFG_WAITED_REQ);
            g_pstWaitedList[i]->last_update_time = 0;
            g_pstWaitedList[i]->file_num = 0;
            g_pstWaitedList[i]->state = 0;
            pthread_mutex_init(&g_pstWaitedList[i]->pMutex, NULL);
        }
    }

    return 0;
}

void waitedList_free(void)
{
    int i;
    for(i=0;i<MAX_REMAIN_FILE;i++)
    {
        if(g_pstWaitedList[i] != NULL)
        {
            waitedList_clean(i);
            pthread_mutex_destroy(&g_pstWaitedList[i]->pMutex);
            free(g_pstWaitedList[i]);
            g_pstWaitedList[i] = NULL;
        }
    }

    return;
}

void waitedList_clean(int num)
{printf("waited clean :%d\n", num);
    if(g_pstWaitedList[num]->uiListSize == 0)
    {
        return;
    }

    stWaitedNode *pNode = g_pstWaitedList[num]->pFront;
    pthread_mutex_lock(&g_pstWaitedList[num]->pMutex);
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
    g_pstWaitedList[num]->pFront = NULL;
    g_pstWaitedList[num]->pRear = NULL;
    g_pstWaitedList[num]->uiListSize = 0;
    g_pstWaitedList[num]->uiMsgLen = 0;
    g_pstWaitedList[num]->last_update_time = 0;
    g_pstWaitedList[num]->file_num = 0;
    g_pstWaitedList[num]->state = 0;
    pthread_mutex_unlock(&g_pstWaitedList[num]->pMutex);

    return;
}

int find_suit_waitedList(int FILE_NUM)    //在已有链中搜索 若没有则找空链 再没有挤掉旧链
{printf("waited findsuitlist :%d\n", FILE_NUM);
    int i, suit_num = 0, min_time = g_pstWaitedList[0]->last_update_time;
    for(i=0;i<MAX_REMAIN_FILE;i++)
    {printf("find suit state:%d, file_num\n", g_pstWaitedList[i]->state, g_pstWaitedList[i]->file_num);
        if(g_pstWaitedList[i]->state == 1 && FILE_NUM == g_pstWaitedList[i]->file_num)
        {printf("waited findsuitnum:%d\n", i);
            return i;
        }
    }
    for(i=0;i<MAX_REMAIN_FILE;i++)
    {
        if(g_pstWaitedList[i]->state == 0)
        {printf("waited findsuitnum:%d\n", i);
            return i;
        }
    }
    for(i=1;i<MAX_REMAIN_FILE;i++)
    {
        if(g_pstWaitedList[i]->last_update_time < min_time)
        {
            min_time = g_pstWaitedList[i]->last_update_time;
            suit_num = i;
        }
    }
    waitedList_clean(suit_num);printf("waited findsuitnum:%d\n", suit_num);
    return suit_num;
}

int waitedList_push(void *pData, int iDataLen, unsigned int uiWaitedID)
{printf("waited push pdata:%d, idatalen:%d, uiwaitedID:%d\n", pData, iDataLen, uiWaitedID);
    stWaitedNode *pprevNode, *pnextNode, *pNode = (stWaitedNode *)malloc(sizeof(stWaitedNode));
    char *sID = (char *)&uiWaitedID;
    if(pNode == NULL)
    {
        return -1;
    }

    int FILE_NUM = sID[0];
    int LIST_NUM = find_suit_waitedList(FILE_NUM);
    pthread_mutex_lock(&g_pstWaitedList[LIST_NUM]->pMutex);
    pNode->cSendTimers = 0;
    pNode->uiWaitedID = uiWaitedID; //每个配置的ID
    pNode->iDataLen = iDataLen;
    pNode->pData = malloc(iDataLen);
    memcpy(pNode->pData, pData, iDataLen);

    //选择排序
    printf("sID[0]:%d, list_num:%d, list_size:%d\n", sID[0], LIST_NUM, g_pstWaitedList[LIST_NUM]->uiListSize);
    if(g_pstWaitedList[LIST_NUM]->state == 0)
    {
        pNode->pPrev = NULL;
        pNode->pNext = NULL;
        g_pstWaitedList[LIST_NUM]->pFront = pNode;
        g_pstWaitedList[LIST_NUM]->pRear = pNode;
        g_pstWaitedList[LIST_NUM]->state = 1;
        g_pstWaitedList[LIST_NUM]->file_num = FILE_NUM;
    }
    else
    {
        if(g_pstWaitedList[LIST_NUM]->uiListSize == 0)
        {
            pNode->pPrev = NULL;
            pNode->pNext = NULL;
            g_pstWaitedList[LIST_NUM]->pFront = pNode;
            g_pstWaitedList[LIST_NUM]->pRear = pNode;
        }
        else
        {
            pprevNode = g_pstWaitedList[LIST_NUM]->pFront;
            pnextNode = pprevNode->pNext;
            if(pprevNode->uiWaitedID > uiWaitedID)
            {
                pNode->pPrev = NULL;
                pNode->pNext = pprevNode;
                g_pstWaitedList[LIST_NUM]->pFront = pNode;
            }
            else if(pprevNode->uiWaitedID == uiWaitedID)
            {
                return 0;
            }
            else
            {
                while(pnextNode)
                {
                    if(pprevNode->uiWaitedID < uiWaitedID)
                    {
                        if(pnextNode->uiWaitedID > uiWaitedID)
                        {
                            pNode->pPrev = pprevNode;
                            pNode->pNext = pnextNode;
                            pprevNode->pNext = pNode;
                            pnextNode->pPrev = pNode;
                            break;
                        }
                        else if(pnextNode->uiWaitedID == uiWaitedID)
                        {
                            return 0;
                        }
                        else
                        {
                            pprevNode = pnextNode;
                            pnextNode = pnextNode->pNext;
                        }
                    }
                    else
                    {
                        return -1;
                    }
                }
                pprevNode->pNext = pNode;
                g_pstWaitedList[LIST_NUM]->pRear = pNode;
                pNode->pPrev = pprevNode;
                pNode->pNext = NULL;
            }
        }
    }
    g_pstWaitedList[LIST_NUM]->uiListSize++;
    g_pstWaitedList[LIST_NUM]->uiMsgLen += (sizeof(DATA_NEWCFG) + iDataLen);
    g_pstWaitedList[LIST_NUM]->last_update_time = time(NULL);

    if(sID[2] == 1)
    {
        if(sID[1] + 1 == g_pstWaitedList[LIST_NUM]->uiListSize)
        {
            g_pstWaitedList[LIST_NUM]->state = 2;
        }
    }
    pthread_mutex_unlock(&g_pstWaitedList[LIST_NUM]->pMutex);
    printf("uilistsize:%d uiMsgLen:%d last_update_time:%d file_num:%d state:%d\n",
           g_pstWaitedList[LIST_NUM]->uiListSize,
           g_pstWaitedList[LIST_NUM]->uiMsgLen,
           g_pstWaitedList[LIST_NUM]->last_update_time,
           g_pstWaitedList[LIST_NUM]->file_num,
           g_pstWaitedList[LIST_NUM]->state);

    return 0;
}

int __waitedList_delete(int list_num, stWaitedNode *pNode)
{printf("get into waited delete\n");
    pthread_mutex_lock(&g_pstWaitedList[list_num]->pMutex);
    g_pstWaitedList[list_num]->uiListSize--;
    g_pstWaitedList[list_num]->uiMsgLen -= (sizeof(DATA_NEWCFG) + pNode->iDataLen);
    if(g_pstWaitedList[list_num]->uiListSize == 0)
    {
        g_pstWaitedList[list_num]->pFront = NULL;
        g_pstWaitedList[list_num]->pRear = NULL;
    }
    else
    {
        if(pNode == g_pstWaitedList[list_num]->pFront)
        {
            g_pstWaitedList[list_num]->pFront = pNode->pNext;
        }

        if(pNode == g_pstWaitedList[list_num]->pRear)
        {
            g_pstWaitedList[list_num]->pRear = pNode->pPrev;
        }
    }

    if(pNode->pPrev != NULL && pNode->pNext != NULL)
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }
    free(pNode);
    pthread_mutex_unlock(&g_pstWaitedList[list_num]->pMutex);

    return 0;
}

int waitedList_file(int list_num)
{printf("get into waited file\n");
    stWaitedNode *pNode = g_pstWaitedList[list_num]->pFront;
    int i, fd, group;
    if(g_pstWaitedList[list_num]->state != 2)
    {
        return 0;
    }
    char *pcFilenameWaited = (char *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcFilenameWaited, 0, MAX_STDIN_FILE_LEN);
    group = g_pstWaitedList[list_num]->file_num;
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
    }while(pNode);
    waitedList_clean(list_num);
    close(fd);
    free(pcFilenameWaited);
    return 0;
}
int waitedList_findAndDelete(unsigned int uiTargetDataID)    //不使用
{
    stWaitedNode *pNode;
    int i, file_num = *(char *)&uiTargetDataID;
    for(i=0;i<MAX_REMAIN_FILE;i++)
    {
        if(g_pstWaitedList[i]->state == 1 && file_num == g_pstWaitedList[i]->file_num)
        {
            break;
        }
    }
    if (i == MAX_REMAIN_FILE)
    {
        //error
        log_warning("waitedList_findAndDelete uiTargetDataID(%u) failed!", uiTargetDataID);
        return -1;
    }
    pNode = g_pstWaitedList[i]->pFront;
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
            __waitedList_delete(i, pNode);

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

unsigned int waitedList_getListSize(int list_num)
{
    return g_pstWaitedList[list_num]->uiListSize;
}

unsigned int waitedList_getMsgLen(int list_num)
{
    return g_pstWaitedList[list_num]->uiMsgLen;
}

stWaitedNode *waitedList_getFrontNode(int list_num)
{
    return g_pstWaitedList[list_num]->pFront;
}

stWaitedNode *waitedList_getRearNode(int list_num)
{
    return g_pstWaitedList[list_num]->pRear;
}




