#ifndef _WAITED_LIST_H_
#define _WAITED_LIST_H_

#include <pthread.h>
#include "protocol.h"

#define MAX_STDIN_FILE_LEN 128
#define MAX_REMAIN_FILE 10

typedef struct WaitedNode
{
    char cSendTimers;
    unsigned int uiWaitedID;
    int iDataLen;
    void *pData;
    struct WaitedNode *pPrev;
    struct WaitedNode *pNext;
}stWaitedNode;

typedef struct WaitedList
{
    stWaitedNode *pFront;
    stWaitedNode *pRear;
    unsigned int uiListSize; //链表中的节点个数
    unsigned int uiMsgLen; //如果将链表中的所有节点以MSG_NEWCFG_WAITED_REQ发送的长度
    int last_update_time;
    int file_num;
    int state;           //0表示未占用 1表示正在使用 2表示已完成
    pthread_mutex_t pMutex;
}stWaitedList;


int waitedList_init(void);
void waitedList_free(void);
void waitedList_clean(int);

int waitedList_push(void *pData, int iDataLen, unsigned int uiWaitedID);
int waitedList_findAndDelete(unsigned int uiTargetDataID);
int waitedList_file(int);

unsigned int waitedList_getListSize(int);
unsigned int waitedList_getMsgLen(int);
stWaitedNode *waitedList_getFrontNode(int);
stWaitedNode *waitedList_getRearNode(int);

#endif //_WAITED_LIST_H_
