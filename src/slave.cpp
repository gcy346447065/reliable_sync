#include <unistd.h> //for STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <sys/socket.h> //for recv
#include <netinet/in.h> //for htons
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

void *slave_alloc(WORD wBufLen)
{
    void *pRecvBuf = malloc(wBufLen);
    if(pRecvBuf == NULL)
    {
        return NULL;
    }
    memset(pRecvBuf, 0, wBufLen);
    return pRecvBuf;
}

DWORD slave_free(void *pBuf)
{
    free(pBuf);
    return SUCCESS;
}

DWORD slave_mailboxProc(void *pSlv)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    //log_debug(byLogNum, "slave_mailboxProc().");

    void *pRecvBuf = slave_alloc(MAX_RECV_LEN);
    if(pRecvBuf == NULL)
    {
        log_error(byLogNum, "slave_alloc error!");
        slave_free(pRecvBuf);
        return FAILE;
    }

    WORD wBufLen = MAX_RECV_LEN;
    dwRet = slave_recv(pSlv, pRecvBuf, &wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave_recv error!");
        slave_free(pRecvBuf);
        return FAILE;
    }

    //log_hex(byLogNum, pRecvBuf, wBufLen);
    dwRet = slave_msgHandle(pSlv, pRecvBuf, wBufLen);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave_MsgHandle error!");
        slave_free(pRecvBuf);
        return FAILE;
    }

    slave_free(pRecvBuf);
    return dwRet;
}

DWORD slave_keepaliveTimerProc(void *pSlv)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    
    log_debug(byLogNum, "slave keep alive time up!");
    
    //向master发送自己的KeepAlive心跳报文
    MSG_KEEP_ALIVE_REQ_S *pstKeepAlivePkg = (MSG_KEEP_ALIVE_REQ_S *)slave_alloc_reqMsg(pclsSlv->wSlvAddr, pclsSlv->wMstAddr, START_SIG_2, CMD_KEEP_ALIVE);

    slave_sendMsg((void *)pclsSlv, pclsSlv->wMstAddr, (void *)pstKeepAlivePkg, sizeof(MSG_KEEP_ALIVE_REQ_S));

    dwRet = pclsSlv->pKeepAliveTimer->start(KEEPALIVE_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsSlv->pKeepAliveTimer start error!");
        return FAILE;
    }
    
    return SUCCESS;
}

DWORD slave_batchTimerProc(void *pSlv)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    
    log_debug(byLogNum, "slave batch time up!");

    if(pclsSlv->stBatch.byBatchFlag == TRUE)
    {
        DWORD dwRet = slave_batchRes2Mst(pSlv);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "slave failed to response batch to master!");
            return FAILE;
        }
    }
    
    dwRet = pclsSlv->pBatchTimer->start(NEWCFG_BATCH_FAST_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsSlv->pBatchTimer start error!");
        return FAILE;
    }

    return SUCCESS;
}

DWORD slave_initTimer(void *pArg)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pArg;
    BYTE byLogNum = pclsSlv->byLogNum;

    //KeepAlive定时器
    pclsSlv->pKeepAliveTimer = new timer(byLogNum);
    dwRet = pclsSlv->pKeepAliveTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsSlv->pKeepAliveTimer init error!");
        return FAILE;
    }
    
    //Batch定时器
    pclsSlv->pBatchTimer = new timer(byLogNum);
    dwRet = pclsSlv->pBatchTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsSlv->pBatchTimer init error!");
        return FAILE;
    }
    

    return SUCCESS;
}

DWORD slave_startTimer(void *pArg)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pArg;
    BYTE byLogNum = pclsSlv->byLogNum;
    
    dwRet = pclsSlv->pKeepAliveTimer->start(KEEPALIVE_TIMER_VALUE); // 1min -> 6s
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsSlv->pKeepAliveTimer start error!");
        return FAILE;
    }
    
    return SUCCESS;
}

DWORD slave::slave_Init()
{
    log_init(byLogNum, "");//slave的log输出到/var/log/local2.log文件
    log_debug(byLogNum, "Slave Task Beginning.");
    DWORD dwRet = SUCCESS;

    stBatch.pbyBitmap = NULL;
    stBatch.vecDataIDs.clear();

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
    log_debug(byLogNum, "wMstAddr(%d), wSlvAddr(%d).", wMstAddr, wSlvAddr);
    dwRet = pDmm->create_mailbox(&pMbufer, wSlvAddr, "slv_mb");
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "create_mailbox error!");
        return FAILE;
    }

    dwRet = pVos->vos_RegTask("slv_mb", pMbufer->dwSocketFd, slave_mailboxProc, this);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_RegTask error!");
        return FAILE;
    }

    /* 向master发送一次登录包，以方便master记录wSlvAddr  */
    MSG_LOGIN_REQ_S *pstLogin = (MSG_LOGIN_REQ_S *)slave_alloc_reqMsg(wSlvAddr, wMstAddr, START_SIG_2, CMD_LOGIN);
    log_debug(byLogNum, "wSlvAddr(%d) ready to send login msg to master.", wSlvAddr);
    if(!pstLogin)
    {
        log_error(byLogNum, "slave_allocLogin error!");
        return FAILE;
    }
    //log_hex_8(byLogNum, pstLogin, 16);
    dwRet = slave_sendMsg((void *)this, wMstAddr, pstLogin, sizeof(MSG_LOGIN_REQ_S));
    if(dwRet == FAILE)
    {
        log_error(byLogNum, "slave_sendMsg error(%u)!", dwRet);
        return FAILE;
    }

    //slave的定时器
    dwRet |= slave_initTimer(this);

    dwRet |= pVos->vos_RegTask("slv_ka_timer", pKeepAliveTimer->dwTimerFd, slave_keepaliveTimerProc, this);
    dwRet |= pVos->vos_RegTask("slv_batch_timer", pBatchTimer->dwTimerFd, slave_batchTimerProc, this);
    
    dwRet |= slave_startTimer(this);
    if(dwRet == FAILE)
    {
        log_error(byLogNum, "Slave set timer error(%u)!", dwRet);
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
    log_debug(byLogNum, "slave_Loop begin.");
    pVos->vos_EpollWait(); //while(1)!!!
    return;
}

