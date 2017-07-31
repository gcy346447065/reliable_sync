#include <stdlib.h> //for malloc
#include "list_data.h"
#include "log.h"

static DATA_LIST_S *g_pstDataList;

DWORD list_data::data_init(void)
{
    g_pstDataList = (DATA_LIST_S *)malloc(sizeof(DATA_LIST_S));
    if(g_pstDataList != NULL)
    {
        g_pstDataList->pFront = NULL;
        g_pstDataList->pRear = NULL;
        g_pstDataList->dwDataCount = 0;
        g_pstDataList->dwBatchCount = 0;
        g_pstDataList->dwInstantCount = 0;
        g_pstDataList->dwWaitedCount = 0;
    }

    return SUCCESS;
}

DWORD list_data::data_free(void)
{
    if(g_pstDataList != NULL)
    {
        data_clean();
        free(g_pstDataList);
        g_pstDataList = NULL;
    }

    return SUCCESS;
}

DWORD list_data::data_clean(void)
{
    if(g_pstDataList->dwDataCount == 0)
    {
        return SUCCESS;
    }

    DATA_NODE_S *pNode = g_pstDataList->pFront;
    while(pNode->pNext != NULL)
    {
        pNode = pNode->pNext;
        free(pNode->pPrev);
        pNode->pPrev = NULL;
    }
    free(pNode);

    return SUCCESS;
}

DWORD list_data::data_insert(BYTE byDataType, WORD wDataID)
{
    return SUCCESS;
}
