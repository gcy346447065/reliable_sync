#include <unistd.h> //for read STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <sys/socket.h> //for recv
#include "macro.h"
#include "log.h"
#include "event.h"
#include "timer.h"
#include "vos.h"
#include "mbufer.h"
#include "master.h"
#include "master_send.h"
#include "master_recv.h"
#include "protocol.h"

static vos *g_pMasterVos;
static dmm *g_pMasterDmm;
mbufer *g_pMstMbufer;
timer *g_pKeepaliveTimer;

DWORD master_stdinProc(void *pObj)
{
    DWORD dwRet = SUCCESS;

    log_debug("master_stdinProc()");

    return dwRet;
}

DWORD master_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_mailboxProc()");

    BYTE *pbyRecvBuf = master_alloc_RecvBuffer(MAX_BUFFER_SIZE);
    if(pbyRecvBuf == NULL)
    {
        log_error("master_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_BUFFER_SIZE;
    dwRet = master_recv(pbyRecvBuf, &wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("master_recv error!");
        return FAILE;
    }

    log_hex(pbyRecvBuf, wBufLen);

    dwRet = master_msgHandle(pbyRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("master_MsgHandle error!");
        return FAILE;
    }

    return dwRet;
}

DWORD master_keepaliveTimerProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_keepaliveTimerProc()");

    //查看备机注册表中各备机的keepalive发送次数，如大于3次则解注册该备机
    for(INT i = 0; i < g_pMstMbufer->g_bySlvNums; i++)//可能出现备机过多注册不上的情况
    {
        if(g_pMstMbufer->g_abySlvKeepailveSendTimes[i] < 3)
        {
            //发送次数未超三次，向该备机地址发送保活消息
            log_info("Send keepailve msg to bySlvAddr(%d).", g_pMstMbufer->g_abySlvAddrs[i]);
            g_pMstMbufer->g_abySlvKeepailveSendTimes[i]++;
        }
        else
        {
            //发送次数超过三次，解注册该备机地址

        }
    }

    dwRet = g_pKeepaliveTimer->start(KEEPALIVE_TIMER_VALUE);//3min
    if(dwRet != SUCCESS)
    {
        log_error("g_pKeepaliveTimer->start error!");
        return FAILE;
    }
    
    return dwRet;
}

DWORD master_InitAndLoop(BYTE byMasterAddr)
{
    DWORD dwRet = SUCCESS;
    log_debug("byMasterAddr(%d).", byMasterAddr);

    g_pMstMbufer = new mbufer;
    g_pMstMbufer->g_byMstAddr = byMasterAddr;//实际只使用该位对应ip加端口号
    g_pMstMbufer->g_bySlvNums = 0;
    memset(g_pMstMbufer->g_abySlvAddrs, 0, sizeof(g_pMstMbufer->g_abySlvAddrs));
    memset(g_pMstMbufer->g_abySlvKeepailveSendTimes, 0, sizeof(g_pMstMbufer->g_abySlvKeepailveSendTimes));

    /* 初始化事件调用机制vos（用epoll模拟实现） */
    g_pMasterVos = new vos;
    dwRet = g_pMasterVos->VOS_Init();
    if(dwRet != SUCCESS)
    {
        log_error("VOS_Init error!");
        return FAILE;
    }

    /* 初始化网络通信mbufer，并创建邮箱 */
    g_pMasterDmm = new dmm;//实际上在create_mailbox中确定mbufer中的g_dwSocketFd，也就是对应vos中的dwTaskEventFd
    dwRet = g_pMasterDmm->create_mailbox(&g_pMstMbufer, byMasterAddr);
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }

    /* 将mbufer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pMasterVos->VOS_RegTaskEventFd(VOS_TASK_MASTER_MAILBOX, g_pMstMbufer->g_dwSocketFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pMasterVos->VOS_RegTaskFunc(VOS_TASK_MASTER_MAILBOX, master_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    /* 将STDIN_FILENO添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pMasterVos->VOS_RegTaskEventFd(VOS_TASK_MASTER_STDIN, STDIN_FILENO);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pMasterVos->VOS_RegTaskFunc(VOS_TASK_MASTER_STDIN, master_stdinProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    g_pKeepaliveTimer = new timer;
    dwRet = g_pKeepaliveTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error("g_pKeepaliveTimer->init error!");
        return FAILE;
    }
    
    /* 将pRegisterTimer添加到vos中，需要利用Macro关联EventFd和Func */
    dwRet = g_pMasterVos->VOS_RegTaskEventFd(VOS_TASK_MASTER_KEEPAILVE_TIMER, g_pKeepaliveTimer->g_dwTimerFd);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskEventFd error!");
        return FAILE;
    }
    dwRet = g_pMasterVos->VOS_RegTaskFunc(VOS_TASK_MASTER_KEEPAILVE_TIMER, master_keepaliveTimerProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("VOS_RegTaskFunc error!");
        return FAILE;
    }

    dwRet = g_pKeepaliveTimer->start(KEEPALIVE_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error("g_pKeepaliveTimer->start error!");
        return FAILE;
    }

    /* 进入vos循环 */
    g_pMasterVos->VOS_EpollWait(); //while(1)!!!

    return dwRet;
}

DWORD master_Free()
{
    delete g_pMasterVos;
    delete g_pMstMbufer;
    delete g_pMasterDmm;

    return SUCCESS;
}

