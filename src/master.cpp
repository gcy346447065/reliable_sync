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

DWORD dwMasterHEHE = 0;

static vos *g_pMasterVos;
static dmm *g_pMasterDmm;
mbufer *g_pMstMbufer;
timer *g_pKeepaliveTimer;

DWORD master_stdinProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_stdinProc(), dwMasterHEHE(%lu)", dwMasterHEHE);
    dwMasterHEHE = 0;//用于统计短时间收到的消息包的数目

    return dwRet;
}

DWORD master_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    //log_debug("master_mailboxProc()");

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
    if(pbyRecvBuf == NULL || wBufLen == 0)
    {
        log_error("pbyRecvBuf or wBufLen error!");
        return FAILE;
    }

    //log_hex(pbyRecvBuf, wBufLen);
    dwRet = master_msgHandle(pbyRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error("master_msgHandle error!");
        return FAILE;
    }

    return dwRet;
}

DWORD master_keepaliveTimerProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_keepaliveTimerProc()");
    //log_debug("slv_getSlvNum(%d)", g_pMstMbufer->g_pSlvList->slv_getSlvNum());

    MSG_KEEP_ALIVE_REQ_S *pstReq = (MSG_KEEP_ALIVE_REQ_S *)master_alloc_reqMsg(0, CMD_KEEP_ALIVE);//！这里没有写入备机地址以方便复用
    if(!pstReq)
    {
        log_error("master_alloc_reqMsg error!");
        return FAILE;
    }

    //查看备机注册表中各备机的保活发送次数，如大于3次则解注册该备机，否则发送保活包
    BYTE *pbyRetSlvAddrs = (BYTE *)malloc(sizeof(BYTE) * g_pMstMbufer->g_pSlvList->slv_getSlvNum());
    dwRet = g_pMstMbufer->g_pSlvList->slv_traverseAndRetSlvAddr(pbyRetSlvAddrs);//slv_getSlvNum该值会在其中更新
    if(dwRet != SUCCESS)
    {
        log_error("slv_traverseAndRetSlvAddr error!");
        free(pstReq);
        return FAILE;
    }

    //log_debug("slv_getSlvNum(%d)", g_pMstMbufer->g_pSlvList->slv_getSlvNum());
    for(UINT i = 0; i < g_pMstMbufer->g_pSlvList->slv_getSlvNum(); i++)
    {
        pstReq->stMsgHeader.byDstAddr = pbyRetSlvAddrs[i];

        dwRet = master_sendToOne(pbyRetSlvAddrs[i], (BYTE *)pstReq, sizeof(MSG_KEEP_ALIVE_REQ_S));
        if(dwRet != SUCCESS)
        {
            log_error("master_sendToOne error!");
            free(pbyRetSlvAddrs);
            free(pstReq);
            return FAILE;
        }
    }

    free(pbyRetSlvAddrs);
    free(pstReq);

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
    g_pMstMbufer->g_bySlvAddr = 0;//主机中用不到
    g_pMstMbufer->g_pSlvList = new list_slv;//使用备机地址链表进行管理
    dwRet = g_pMstMbufer->g_pSlvList->slv_init();
    if(dwRet != SUCCESS)
    {
        log_error("slv_init error!");
        return FAILE;
    }

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
    g_pMstMbufer->g_pSlvList->slv_free();
    delete g_pMstMbufer->g_pSlvList;

    delete g_pMasterVos;
    g_pMasterDmm->delete_mailbox(g_pMstMbufer);
    delete g_pMstMbufer;
    delete g_pMasterDmm;

    return SUCCESS;
}

