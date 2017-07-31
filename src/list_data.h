#ifndef _LIST_DATA_H_
#define _LIST_DATA_H_

#include "macro.h"
#include "protocol.h"

typedef struct tagDATA_NODE
{
    BYTE byDataType;
    WORD wDataID;
    MSG_DATA_S stData;
    struct tagDATA_NODE *pPrev;
    struct tagDATA_NODE *pNext;
}DATA_NODE_S;

typedef struct tagDATA_LIST
{
    DATA_NODE_S *pFront;
    DATA_NODE_S *pRear;
    DWORD dwDataCount;
    DWORD dwBatchCount;
    DWORD dwInstantCount;
    DWORD dwWaitedCount;
}DATA_LIST_S;

/*
 * 该链是以bySlvAddr按从小到大顺序放入的单链表，可能会在遍历链表时删除节点
 */
class list_data
{

public:
    DWORD data_init(void);
    DWORD data_free(void);
    DWORD data_clean(void);

    DWORD data_insert(BYTE byDataType, WORD wDataID);
    DATA_NODE_S *data_find(BYTE byDataType, WORD wDataID);
    DWORD data_delete(DATA_NODE_S *pNode);
    DWORD data_traverseAndRetSlvAddr(BYTE *pbyRetSlvAddrs);
    
    DWORD data_getSlvNum(void);
    DWORD data_resetKeepaliveSendTimes(BYTE bySlvAddr);
};

#endif //_LIST_DATA_H_
