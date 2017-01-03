#ifndef _LIST_H_
#define _LIST_H_

#include <pthread.h>

typedef struct Node
{
    int iFindTimers;
    int iSendTimers;
    int iDataID;
    int iDataLen;
    void *pData;
    struct Node *pPrev;
    struct Node *pNext;
}stNode;

typedef struct List
{
    stNode *pFront;
    stNode *pReadNow;
    stNode *pRear;
    int iSize;
    int iReadSize;
    pthread_mutex_t pMutex;
}stList;


stList *list_init();
int list_push(stList *pstList, void *pData, int iDataLen);
int list_read(stList *pstList);
int list_deleteByDataID(stList *pstList, int iTargetDataID);
int list_deleteByNode(stList *pstList, stNode *pTargetNode);
stNode *list_find(stList *pstList, int iTargetDataID);
void list_free(stList *pstList);
void list_clean(stList *pstList);
int list_isEmpty(stList *pstList);
int list_getSize(stList *pstList);
int list_getReadSize(stList *pstList);

#endif //_LIST_H_
