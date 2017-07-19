#ifndef _LIST_SLV_H_
#define _LIST_SLV_H_

#include <pthread.h>
#include "macro.h"

typedef struct tagSLV_NODE
{
    BYTE bySlvAddr;
    BYTE bySlvKeepaliveSendTimes;
    struct tagSLV_NODE *pPrev;
    struct tagSLV_NODE *pNext;
}SLV_NODE_S;

typedef struct tagSLV_LIST
{
    SLV_NODE_S *pFront;
    SLV_NODE_S *pRear;
    BYTE bySlvNum;
}SLV_LIST_S;

/*
 * 该链是以bySlvAddr按从小到大顺序放入的单链表，可能会在遍历链表时删除节点
 */
class list_slv
{

public:
    DWORD slv_init(void);
    DWORD slv_free(void);
    DWORD slv_clean(void);

    DWORD slv_insert(BYTE bySlvAddr);
    SLV_NODE_S *slv_find(BYTE bySlvAddr);
    DWORD slv_delete(SLV_NODE_S *pNode);
    DWORD slv_traverseAndRetSlvAddr(BYTE *pbyRetSlvAddrs);
    
    DWORD slv_getSlvNum(void);
    DWORD slv_resetKeepaliveSendTimes(BYTE bySlvAddr);
};

#endif //_LIST_SLVADDR_H_
