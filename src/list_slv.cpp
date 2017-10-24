#include <stdlib.h> //for malloc
#include "list_slv.h"
#include "log.h"

static SLV_LIST_S *g_pstSlvAddrList;

DWORD list_slv::slv_init(void)
{
    g_pstSlvAddrList = (SLV_LIST_S *)malloc(sizeof(SLV_LIST_S));
    if(g_pstSlvAddrList != NULL)
    {
        g_pstSlvAddrList->pFront = NULL;
        g_pstSlvAddrList->pRear = NULL;
        g_pstSlvAddrList->bySlvNum = 0;
    }

    return SUCCESS;
}

DWORD list_slv::slv_free(void)
{
    if(g_pstSlvAddrList != NULL)
    {
        slv_clean();
        free(g_pstSlvAddrList);
        g_pstSlvAddrList = NULL;
    }

    return SUCCESS;
}

DWORD list_slv::slv_clean(void)
{
    if(g_pstSlvAddrList->bySlvNum == 0)
    {
        return SUCCESS;
    }

    SLV_NODE_S *pNode = g_pstSlvAddrList->pFront;
    while(pNode->pNext != NULL)
    {
        pNode = pNode->pNext;
        free(pNode->pPrev);
        pNode->pPrev = NULL;
    }
    free(pNode);

    return SUCCESS;
}

DWORD list_slv::slv_insert(BYTE bySlvAddr)
{
    log_debug("slv_insert.");

    SLV_NODE_S *pNode = (SLV_NODE_S *)malloc(sizeof(SLV_NODE_S));
    if(pNode == NULL)
    {
        log_error("malloc error!");
        return FAILE;
    }
    pNode->bySlvAddr = bySlvAddr;
    pNode->bySlvKeepaliveSendTimes = 0;

    //链表为空，节点直接放
    if(g_pstSlvAddrList->bySlvNum == 0)
    {
        pNode->pPrev = NULL;
        pNode->pNext = NULL;
        g_pstSlvAddrList->pFront = pNode;
        g_pstSlvAddrList->pRear = pNode;
        g_pstSlvAddrList->bySlvNum++;
        return SUCCESS;
    }

    //节点放在链表头前
    if(g_pstSlvAddrList->pFront->bySlvAddr > bySlvAddr)
    {
        pNode->pPrev = NULL;
        pNode->pNext = g_pstSlvAddrList->pFront;
        g_pstSlvAddrList->pFront->pPrev = pNode;
        g_pstSlvAddrList->pFront = pNode;
        g_pstSlvAddrList->bySlvNum++;
        return SUCCESS;
    }

    //节点放在链表尾后
    if(g_pstSlvAddrList->pRear->bySlvAddr < bySlvAddr)
    {
        pNode->pPrev = g_pstSlvAddrList->pRear;
        pNode->pNext = NULL;
        g_pstSlvAddrList->pRear->pNext = pNode;
        g_pstSlvAddrList->pRear = pNode;
        g_pstSlvAddrList->bySlvNum++;
        return SUCCESS;
    }

    //节点放在链表中间，或者节点重复不加入
    SLV_NODE_S *pTmpNode = g_pstSlvAddrList->pFront;
    while(pTmpNode != NULL)
    {
        if(pTmpNode->bySlvAddr < bySlvAddr)
        {
            //节点的备机地址比目标地址小，继续查找
            pTmpNode = pTmpNode->pNext;
        }
        else if(pTmpNode->bySlvAddr == bySlvAddr)
        {
            //节点的备机地址与目标地址相同，说明已经登录过了
            log_debug("This bySlvAddr(%u) has logged.", bySlvAddr);

            free(pNode);
            return SLV_HAS_REGED;
        }
        else if(pTmpNode->bySlvAddr > bySlvAddr)
        {
            //节点的备机地址比目标地址大，说明目标地址可以插在节点前面
            log_debug("This bySlvAddr(%u) should insert.", bySlvAddr);

            pNode->pPrev = pTmpNode->pPrev;
            pNode->pNext = pTmpNode;
            pTmpNode->pPrev->pNext = pNode;
            pTmpNode->pPrev = pNode;
            g_pstSlvAddrList->bySlvNum++;
            return SUCCESS;
        }
    }

    return SUCCESS;
}

SLV_NODE_S *list_slv::slv_find(BYTE bySlvAddr)
{
    SLV_NODE_S *pNode = g_pstSlvAddrList->pFront;
    while(pNode != NULL)
    {
        if(pNode->bySlvAddr < bySlvAddr)
        {
            pNode = pNode->pNext;
        }
        else if(pNode->bySlvAddr == bySlvAddr)
        {
            break;
        }
        else if(pNode->bySlvAddr > bySlvAddr)
        {
            pNode = NULL;
            break;
        }
    }

    return pNode;
}

DWORD list_slv::slv_delete(SLV_NODE_S *pNode)
{
    if(pNode == NULL)
    {
        log_error("pNode is NULL!");
        return FAILE;
    }

    if(pNode->pPrev == NULL && pNode->pNext == NULL)//删除的是最后一个节点
    {
        g_pstSlvAddrList->pFront = NULL;
        g_pstSlvAddrList->pRear = NULL;
    }
    else if(pNode->pPrev == NULL)//删除的是头上的节点
    {
        pNode->pNext->pPrev = NULL;
        g_pstSlvAddrList->pFront = pNode->pNext;
    }
    else if(pNode->pNext == NULL)//删除的是尾上的节点
    {
        pNode->pPrev->pNext = NULL;
        g_pstSlvAddrList->pRear = pNode->pPrev;
    }
    else//删除的是中间的节点
    {
        pNode->pPrev->pNext = pNode->pNext;
        pNode->pNext->pPrev = pNode->pPrev;
    }

    free(pNode);
    g_pstSlvAddrList->bySlvNum--;
    return SUCCESS;
}

DWORD list_slv::slv_traverseAndRetSlvAddr(BYTE *pbyRetSlvAddrs)
{
    SLV_NODE_S *pNode = g_pstSlvAddrList->pFront;
    while(pNode != NULL)
    {
        log_debug("bySlvAddr(%d), bySlvKeepaliveSendTimes(%d)", pNode->bySlvAddr, pNode->bySlvKeepaliveSendTimes);
        if(pNode->bySlvKeepaliveSendTimes >= 3)
        {
            slv_delete(pNode);
        }
        else
        {
            *pbyRetSlvAddrs = pNode->bySlvAddr;
            pbyRetSlvAddrs++;
            pNode->bySlvKeepaliveSendTimes++;
        }

        pNode = pNode->pNext;
    }

    return SUCCESS;
}

DWORD list_slv::slv_getSlvNum(void)
{
    return g_pstSlvAddrList->bySlvNum;
}

DWORD list_slv::slv_resetKeepaliveSendTimes(BYTE bySlvAddr)
{
    SLV_NODE_S *pNode = slv_find(bySlvAddr);
    if(pNode != NULL)
    {
        pNode->bySlvKeepaliveSendTimes = 0;
    }

    return SUCCESS;
}
