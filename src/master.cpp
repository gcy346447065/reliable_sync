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
    log_debug(byLogNum, "master_mailboxProc().");

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

    log_debug(byLogNum, "pRecvBuf(%u).", wBufLen);
    log_hex_8(byLogNum, pRecvBuf, 32);
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

typedef struct
{
    master *pclsMst;
}MASTER_PROCTHREAD_S;

VOID *master_batchProcThread(void *pArg)
{
    MASTER_PROCTHREAD_S *pstProcStruct = (MASTER_PROCTHREAD_S *)pArg;
    master *pclsMst = pstProcStruct->pclsMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_batchProcThread().");

    while(1)
    {
        if(pclsMst->byBatchFlag == TRUE)
        {
            pclsMst->byBatchFlag = FALSE;
            log_debug(byLogNum, "got dwBatchNow(%u).", pclsMst->dwBatchNow);

        }
    }

    return (VOID *)SUCCESS;
}


VOID *master_instantProcThread(void *pArg)
{
    MASTER_PROCTHREAD_S *pstProcStruct = (MASTER_PROCTHREAD_S *)pArg;
    master *pclsMst = pstProcStruct->pclsMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_instantProcThread().");

    map<DWORD, void*>::reverse_iterator rit;
    NODE_DATA_INSTANT_S *pNode;
    WORD wChecksum = 0;
    while(1)
    {
        if(pclsMst->byInstantFlag == TRUE)
        {
            pclsMst->byInstantFlag = FALSE;
            log_debug(byLogNum, "got dwInstantNow(%u).", pclsMst->dwInstantNow);

            //从后往前遍历，进行重发与删除过旧的节点
            for(rit = pclsMst->mapDataInstant.rbegin(); rit != pclsMst->mapDataInstant.rend(); rit++)
            {
                DWORD dwID = (*rit).first;
                log_debug(byLogNum, "dwID(%u)", dwID);
                
                pNode = (NODE_DATA_INSTANT_S *)((*rit).second);
                if(pNode->stState.byIsSucceed == TRUE)
                {
                    //说明该节点发送成功，删除节点
                    log_debug(byLogNum, "is succeed, delete node.");
                }
                else if(pNode->stState.bySendTimes >= MAX_SEND_TIMES)
                {
                    //说明该节点发送不成功次数超过阈值，删除节点
                    log_debug(byLogNum, "send times enough, delete node.");
                }
                else if(pNode->stState.byIsReady == TRUE)
                {
                    //说明该节点准备好，且发送不成功次数未超过阈值，发送节点
                    log_debug(byLogNum, "is ready, send node.");

                    pNode->stState.bySendTimes++;
                    log_debug(byLogNum, "bySendTimes(%u)", pNode->stState.bySendTimes);
                }
                else if(pNode->stState.byIsReady == FALSE)
                {
                    //说明该节点未准备好，计算节点checksum等使其准备好，并发送节点
                    log_debug(byLogNum, "being ready, send node.");
                    wChecksum = checksum((const void *)(pNode->stInstantNet.abyData), (WORD)ntohs(pNode->stInstantNet.wDataLen));
                    //wChecksum = 0;
                    pNode->stInstantNet.wDataChecksum = htons(wChecksum);
                    
                    pNode->stState.byIsReady = TRUE;
                    pNode->stState.bySendTimes++;
                }
            }
        }
    }

    return (VOID *)SUCCESS;
}

VOID *master_waitedProcThread(void *pArg)
{
    MASTER_PROCTHREAD_S *pstProcStruct = (MASTER_PROCTHREAD_S *)pArg;
    master *pclsMst = pstProcStruct->pclsMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_waitedProcThread().");

    while(1)
    {
        if(pclsMst->byWaitedFlag == TRUE)
        {
            pclsMst->byWaitedFlag = FALSE;
            log_debug(byLogNum, "got dwWaitedNow(%u).", pclsMst->dwWaitedNow);
            
            
        }
    }

    return (VOID *)SUCCESS;
}


DWORD master::master_Init()
{
    log_init(byLogNum, "");
    log_debug(byLogNum, "Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    vecSlvAddr.clear();
    mapDataBatch.clear();
    mapDataInstant.clear();
    mapDataWaited.clear();
    byBatchFlag = 0;
    byInstantFlag = 0;
    byWaitedFlag = 0;
    dwBatchNow = 0;
    dwInstantNow = 0;
    dwWaitedNow = 0;

    pVos = new vos(byLogNum);
    dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_Init error!");
        return FAILE;
    }
    pDmm = new dmm(byLogNum);
    pMbufer = new mbufer(byLogNum);

    /* 创建邮箱并注册到vos */
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
    pthread_t ProcThreadId;
    MASTER_PROCTHREAD_S stProcStruct;
    stProcStruct.pclsMst = this;
    INT iRet = pthread_create(&ProcThreadId, NULL, master_batchProcThread, (void *)&stProcStruct);
    if(iRet != SUCCESS)
    {
        log_error(byLogNum, "pthread create error(%d)!", iRet);
        return FAILE;
    }
    iRet = pthread_create(&ProcThreadId, NULL, master_instantProcThread, (void *)&stProcStruct);
    if(iRet != SUCCESS)
    {
        log_error(byLogNum, "pthread create error(%d)!", iRet);
        return FAILE;
    }
    iRet = pthread_create(&ProcThreadId, NULL, master_waitedProcThread, (void *)&stProcStruct);
    if(iRet != SUCCESS)
    {
        log_error(byLogNum, "pthread create error(%d)!", iRet);
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

