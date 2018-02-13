#include <unistd.h> //for read STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <stdio.h> //for sscanf
#include <errno.h> //for errno
#include <pthread.h> //for pthread
#include <netinet/in.h> //for htons
#include "master.h"
#include "macro.h"
#include "protocol.h"
#include "log.h"
#include "master_recv.h"
#include "master_send.h"
#include "checksum.h"
#include "timer.h"

timer *g_pMstBatchTimer;

void *master_alloc(WORD wBufLen)
{
    void *pRecvBuf = malloc(wBufLen);
    if(pRecvBuf == NULL)
    {
        return NULL;
    }
    memset(pRecvBuf, 0, wBufLen);
    return pRecvBuf;
}

void master_free(void *pBuf)
{
    free(pBuf);
    return;
}

DWORD master_mailboxProc(void *pArg)//pArg其实是master *pclsMst
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pArg;
    BYTE byLogNum = pclsMst->byLogNum;
    //log_debug(byLogNum, "master_mailboxProc().");

    void *pRecvBuf = master_alloc(MAX_TASK2MST_RECV_LEN);
    if(pRecvBuf == NULL)
    {
        log_error(byLogNum, "master_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_TASK2MST_RECV_LEN;
    dwRet = master_recv(pArg, pRecvBuf, &wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_recv error!");
        master_free(pRecvBuf);
        return FAILE;
    }
    if(pRecvBuf == NULL || wBufLen == 0)
    {
        log_error(byLogNum, "pbyRecvBuf or wBufLen error!");
        master_free(pRecvBuf);
        return FAILE;
    }

    //log_debug(byLogNum, "pRecvBuf(%u).", wBufLen);
    //log_hex(byLogNum, pRecvBuf, wBufLen);
    //log_hex(byLogNum, pRecvBuf, wBufLen);
    dwRet = master_msgHandle(pArg, pRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master_msgHandle error!");
        master_free(pRecvBuf);
        return FAILE;
    }

    master_free(pRecvBuf);
    return dwRet;
}

DWORD master_keepaliveTimerProc(void *pArg)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pArg;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_keepaliveTimerProc().");
    
    return dwRet;
}

DWORD master_batchTimerProc(void *pMst)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    
    log_debug(byLogNum, "master batch time up!");

    SLAVE_S *pstSlv;
    unsigned int uiSlvIdx;
    DWORD dwBatchFinNums = 0;
    DWORD dwSlaveNums = 0;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        dwSlaveNums++;
        pstSlv = (SLAVE_S *)&(pclsMst->vecSlvs.at(uiSlvIdx));
        if(pstSlv->stBatch.byBatchFlag == TRUE)
        {
            if((pstSlv->stBatch.bySendtimes++) > MAX_SLAVE_RES_BATCH_PKGS)
            {
                log_debug(byLogNum, "Slave(%u)'s retransmit max!", pstSlv->wSlvAddr);
                pstSlv->stBatch.byBatchFlag = FALSE;
                pstSlv->stBatch.bySendtimes = 0;
                pstSlv->stBatch.dwDataNums = 0;
                pstSlv->stBatch.vecDataIDs.clear();
                
                dwBatchFinNums++;
            }
            
            for(DWORD i = 0; i < pstSlv->stBatch.dwDataNums; i++)
            {
                DWORD dwDataID = pstSlv->stBatch.vecDataIDs.at(i);
                NODE_DATA_BATCH_S *pstNodeBatch = pclsMst->mapDataBatch[dwDataID];
                if(!pstNodeBatch)
                {
                    log_error(byLogNum, "master failed to get batch node!");
                    continue;
                }
                
                MSG_DATA_BATCH_REQ_S *pstBatchPkg = (MSG_DATA_BATCH_REQ_S *)master_alloc_dataBatch(pclsMst->wMstAddr, pstSlv->wSlvAddr, pstNodeBatch->stBatchNet.dwDataStart, pstNodeBatch->stBatchNet.dwDataEnd,
                                                                  dwDataID, (void *)pstNodeBatch->stBatchNet.stData.abyData, pstNodeBatch->stBatchNet.stData.wDataLen);

                master_sendMsg((void *)pclsMst, pstSlv->wSlvAddr, (void *)pstBatchPkg, sizeof(MSG_DATA_BATCH_REQ_S) + pstNodeBatch->stBatchNet.stData.wDataLen);

                free(pstBatchPkg);
            }
        }
        else
        {
            dwBatchFinNums++;
        }
    }

    if(dwBatchFinNums < dwSlaveNums)
    {
        //依然有slave处于batch状态
        log_debug(byLogNum, "%u slave(s) still need retransmit!", dwSlaveNums - dwBatchFinNums);
        
        dwRet = g_pMstBatchTimer->start(NEWCFG_BATCH_FAST_TIMER_VALUE); //快速重传定时3s
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "g_pMstBatchTimer->start error!");
            return FAILE;
        }
    }
    else
    {
        //所有slave都batch完毕
        log_debug(byLogNum, "Data batch finished!");
        pclsMst->byBatchFlag = FALSE;
        pclsMst->mapDataBatch.clear();
        
        dwRet = g_pMstBatchTimer->stop();
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "g_pMstBatchTimer->start error!");
            return FAILE;
        }
    }

    return SUCCESS;
}

typedef struct
{
    master *pclsMst;
}MASTER_PROCTHREAD_S;

DWORD master::master_Init()
{
    log_init(byLogNum, "");
    log_debug(byLogNum, "Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    //初始化master的slave列表
    bySlvNums = 0;
    vecSlvs.clear();

    //初始化backup状态
    mapDataBatch.clear();
    mapDataInstant.clear();
    mapDataWaited.clear();
    byBatchFlag = FALSE;
    byInstantFlag = FALSE;
    byWaitedFlag = FALSE;
    
    pVos = new vos(byLogNum);
    dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_Init error!");
        return FAILE;
    }
    pDmm = new dmm(byLogNum);
    pMbufer = new mbufer(byLogNum);

    /* 创建邮箱并注册到vos  */
    log_debug(byLogNum, "wMstAddr(%d).", wMstAddr);
    dwRet = pDmm->create_mailbox(&pMbufer, wMstAddr, "mst_mb");
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("mst_mb", pMbufer->dwSocketFd, master_mailboxProc, this);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_RegTask error!");
        return FAILE;
    }

    /* 创建该子线程主要用于checksum的计算，将io操作与cpu计算分开有利于提高效率 */

    //master的定时器
    g_pMstBatchTimer = new timer(byLogNum);
    dwRet = g_pMstBatchTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "g_pMstBatchTimer->init error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("mst_timer", g_pMstBatchTimer->dwTimerFd, master_batchTimerProc, this);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_RegTask error!");
        return FAILE;
    }
    
    dwRet = g_pMstBatchTimer->start(NEWCFG_BATCH_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "g_pMstBatchTimer->start error!");
        return FAILE;
    }

    return dwRet;
}

VOID master::master_Free()
{
    delete pVos;
    delete pDmm;
    delete pMbufer;

    log_free();
    return;
}

VOID master::master_Loop()
{
    /* 进入vos循环 */
    log_debug(byLogNum, "master_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!
    return;
}

