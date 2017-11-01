#include <unistd.h> //for read STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <stdio.h> //for sscanf
#include <errno.h> //for errno
#include "master.h"
#include "macro.h"
#include "log.h"
#include "master_recv.h"
#include "master_send.h"

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
    log_debug("master_mailboxProc().");

    void *pRecvBuf = master_alloc(MAX_RECV_LEN);
    if(pRecvBuf == NULL)
    {
        log_error("master_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_RECV_LEN;
    dwRet = master_recv(pArg, pRecvBuf, &wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("master_recv error!");
        master_free(pRecvBuf);
        return FAILE;
    }
    if(pRecvBuf == NULL || wBufLen == 0)
    {
        log_error("pbyRecvBuf or wBufLen error!");
        master_free(pRecvBuf);
        return FAILE;
    }

    log_hex(pRecvBuf, 10);
    //log_hex(pRecvBuf, wBufLen);
    dwRet = master_msgHandle(pArg, pRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("master_msgHandle error!");
        master_free(pRecvBuf);
        return FAILE;
    }

    master_free(pRecvBuf);
    return dwRet;
}

DWORD master_keepaliveTimerProc(void *pArg)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_keepaliveTimerProc().");
    
    return dwRet;
}

DWORD master::master_Init()
{
    log_init("MASTER");
    log_debug("Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    vecSlvAddr.clear();
    mapDataBatch.clear();
    mapDataInstant.clear();
    mapDataWaited.clear();

    pVos = new vos;
    dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error("vos_Init error!");
        return FAILE;
    }
    pDmm = new dmm;
    pMbufer = new mbufer;

    /* 创建邮箱并注册到vos */
    log_debug("byMstAddr(%d).", byMstAddr);
    dwRet = pDmm->create_mailbox(&pMbufer, byMstAddr, "mst_mb");
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("mst_mb", pMbufer->dwSocketFd, master_mailboxProc, this);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
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
    log_debug("master_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!
    return;
}

