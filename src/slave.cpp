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

DWORD dwSlaveHEHE = 0;

vos *g_pSlaveVos;
dmm *g_pSlaveDmm;
mbufer *g_pSlvMbufer;
timer *g_pSlvRegTimer;

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

    BYTE *pbyRecvBuf = slave_alloc_RecvBuffer(MAX_BUFFER_SIZE);
    if(pbyRecvBuf == NULL)
    {
        log_error("slave_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_BUFFER_SIZE;
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
    dwRet = g_pSlaveVos->VOS_Init();
    if(dwRet != SUCCESS)
    {
        log_error("VOS_Init error!");
        return FAILE;
    }

    /* 初始化网络通信mbufer，并创建邮箱 */
    g_pSlvMbufer = new mbufer;
    g_pSlvMbufer->g_byMstAddr = byMasterAddr;//实际只使用该位对应ip加端口号
    g_pSlvMbufer->g_bySlvAddr = bySlaveAddr;//实际只使用该位对应ip加端口号
    g_pSlvMbufer->g_pSlvList = NULL;//备机中用不到

    g_pSlaveDmm = new dmm;//实际上在create_mailbox中确定mbufer中的g_dwSocketFd，也就是记录vos中的EventFd
    dwRet = g_pSlaveDmm->create_mailbox(&g_pSlvMbufer, bySlaveAddr);
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }

    dwRet = g_pSlaveVos->VOS_RegTaskEventFd(VOS_TASK_SLAVE_STDIN, STDIN_FILENO);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pSlaveVos->VOS_RegTaskFunc(VOS_TASK_SLAVE_STDIN, slave_stdinProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    /* 将mbufer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pSlaveVos->VOS_RegTaskEventFd(VOS_TASK_SLAVE_MAILBOX, g_pSlvMbufer->g_dwSocketFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pSlaveVos->VOS_RegTaskFunc(VOS_TASK_SLAVE_MAILBOX, slave_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
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
    //log_debug("g_pSlvRegTimer->g_dwTimerFd(%lu)", g_pSlvRegTimer->g_dwTimerFd);
    //g_pSlvRegTimer->get(&dwRet);
    //log_debug("g_pSlvRegTimer->get(%lu)", dwRet);
    
    /* 将pRegisterTimer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pSlaveVos->VOS_RegTaskEventFd(VOS_TASK_SLAVE_REGISTER_TIMER, g_pSlvRegTimer->g_dwTimerFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pSlaveVos->VOS_RegTaskFunc(VOS_TASK_SLAVE_REGISTER_TIMER, slave_registerTimerProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    dwRet = g_pSlvRegTimer->start(REGISTER_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("g_pSlvRegTimer->start error!");
        return FAILE;
    }

    /* 进入vos循环 */
    g_pSlaveVos->VOS_EpollWait(); //while(1)!!!

    return dwRet;
}

DWORD slave_Free()
{
    delete g_pSlaveVos;
    g_pSlaveDmm->delete_mailbox(g_pSlvMbufer);
    delete g_pSlvMbufer;
    delete g_pSlaveDmm;

    g_pSlvRegTimer->free();
    delete g_pSlvRegTimer;

    return SUCCESS;
}

