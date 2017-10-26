#ifndef _LIST_DATA_H_
#define _LIST_DATA_H_

#include "macro.h"
#include "protocol.h"
#include <pthread.h>

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
    pthread_mutex_t pMutex;
    DATA_NODE_S *pFront;
    DATA_NODE_S *pRear;
    DWORD dwAllCount;
    DWORD dwNewCount;
}DATA_LIST_S;


class list_data
{

public:
    DWORD data_init(void);
    DWORD data_free(void);
    DWORD data_clean(void);

    DWORD data_insert(const MSG_DATA_S *pstDataNET);
    DATA_NODE_S *data_find(BYTE byDataType, WORD wDataID);
    DWORD data_delete(DATA_NODE_S *pNode);

    DWORD data_traverseAndPrintBatch(void);
    DWORD data_traverseAndPrintInstant(void);
    DWORD data_traverseAndPrintWaited(void);
    DWORD data_getBatchCount(void);
    DWORD data_getBatchNew(void);
    DWORD data_getInstantCount(void);
    DWORD data_getInstantNew(void);
    DWORD data_getWaitedCount(void);
    DWORD data_getWaitedNew(void);
};

#endif //_LIST_DATA_H_

