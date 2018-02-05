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

typedef struct
{
    master *pclsMst;
}MASTER_PROCTHREAD_S;

DWORD master::master_Init()
{
    log_init(byLogNum, "");
    log_debug(byLogNum, "Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    vecSlvs.clear();
    mapDataBatch.clear();
    mapDataInstant.clear();
    mapDataWaited.clear();

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

