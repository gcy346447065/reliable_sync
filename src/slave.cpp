#include <unistd.h> //for STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <sys/socket.h> //for recv
#include "macro.h"
#include "log.h"
#include "event.h"
#include "timer.h"
#include "vos.h"
#include "mbufer.h"
#include "slave.h"
#include "slave_send.h"
#include "slave_recv.h"
#include "protocol.h"
#include "list_data.h"

DWORD dwSlaveHEHE = 0;

vos *g_pSlaveVos;
dmm *g_pSlaveDmm;
mbufer *g_pSlvMbufer;
timer *g_pSlvRegTimer;

BYTE g_slv_byMstAddr;
BYTE g_slv_bySlvAddr;
list_data *g_slv_pDataList;

DWORD slave_stdinProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("slave_stdinProc(), dwSlaveHEHE(%lu)", dwSlaveHEHE);

    return dwRet;
}

DWORD slave_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("slave_mailboxProc()");

    BYTE *pbyRecvBuf = slave_alloc_RecvBuffer(MAX_RECV_LEN);
    if(pbyRecvBuf == NULL)
    {
        log_error("slave_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_RECV_LEN;
    dwRet = slave_recv(pbyRecvBuf, &wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("slave_recv error!");
        return FAILE;
    }

    log_hex(pbyRecvBuf, wBufLen);

    dwRet = slave_msgHandle(pbyRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("slave_MsgHandle error!");
        return FAILE;
    }

    free(pbyRecvBuf);
    return dwRet;
}

DWORD slave_registerTimerProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("slave_registerTimerProc()");

    /* 拼接主备模块消息体 */
    MSG_LOGIN_REQ_S *pstReq = (MSG_LOGIN_REQ_S *)slave_alloc_reqMsg(CMD_LOGIN);
    if(!pstReq)
    {
        log_error("alloc_slave_reqMsg error!");
        return FAILE;
    }

    dwRet = slave_send((BYTE *)pstReq, sizeof(MSG_LOGIN_REQ_S));
    if(dwRet != SUCCESS)
    {
        log_error("slave_send error!");
        return FAILE;
    }

    free(pstReq);

    dwRet = g_pSlvRegTimer->start(REGISTER_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("g_pSlvRegTimer->start error!");
        return FAILE;
    }
    
    return dwRet;
}

DWORD slave_InitAndLoop(BYTE byMasterAddr, BYTE bySlaveAddr)
{
    DWORD dwRet = SUCCESS;
    log_debug("byMasterAddr(%d), bySlaveAddr(%d).", byMasterAddr, bySlaveAddr);

    /* 初始化事件调用机制vos（用epoll模拟实现） */
    g_pSlaveVos = new vos;
    dwRet = g_pSlaveVos->vos_Init();
    if(dwRet != SUCCESS)
    {
        log_error("VOS_Init error!");
        return FAILE;
    }

    /* 初始化网络通信mbufer，并创建邮箱 */
    g_slv_byMstAddr = byMasterAddr;//实际只使用该位对应ip加端口号
    g_slv_bySlvAddr = bySlaveAddr;//实际只使用该位对应ip加端口号
    g_pSlaveDmm = new dmm;//实际上在create_mailbox中确定mbufer中的g_dwSocketFd，也就是记录vos中的EventFd
    g_pSlvMbufer = new mbufer;
    dwRet = g_pSlaveDmm->create_mailbox(&g_pSlvMbufer, bySlaveAddr, "mailbox");
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }

    dwRet = g_pSlaveVos->vos_RegTask("stdin", STDIN_FILENO, slave_stdinProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 将mbufer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pSlaveVos->vos_RegTask("mailbox", g_pSlvMbufer->dwSocketFd, slave_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 初始化发起注册的定时器 */
    g_pSlvRegTimer = new timer;
    dwRet = g_pSlvRegTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error("g_pSlvRegTimer->init error!");
        return FAILE;
    }
    //log_debug("g_pSlvRegTimer->dwTimerFd(%lu)", g_pSlvRegTimer->dwTimerFd);
    //g_pSlvRegTimer->get(&dwRet);
    //log_debug("g_pSlvRegTimer->get(%lu)", dwRet);
    
    /* 将pRegisterTimer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pSlaveVos->vos_RegTask("regtimer", g_pSlvRegTimer->dwTimerFd, slave_registerTimerProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    dwRet = g_pSlvRegTimer->start(REGISTER_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("g_pSlvRegTimer->start error!");
        return FAILE;
    }

    /* 进入vos循环 */
    g_pSlaveVos->vos_EpollWait(); //while(1)!!!

    return dwRet;
}


DWORD slave::slave_Init()
{
    log_init("SLAVE");
    log_debug("Slave Task Beginning.");
    DWORD dwRet = SUCCESS;

    mapBatchData.clear();
    mapInstantData.clear();
    mapWaitedData.clear();

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
    log_debug("byMstAddr(%d), bySlvAddr(%d).", byMstAddr, bySlvAddr);
    dwRet = pDmm->create_mailbox(&pMbufer, bySlvAddr, "slv_mb");
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("slv_mb", pMbufer->dwSocketFd, slave_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    return dwRet;
}

VOID slave::slave_Free()
{
    delete pVos;
    delete pDmm;
    delete pMbufer;

    log_free();
    return;
}

VOID slave::slave_Loop()
{
    /* 进入vos循环 */
    log_debug("slave_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!
    return;
}


