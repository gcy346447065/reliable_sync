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

mbufer *g_pSlvMbufer;
timer *g_pSlvRegTimer;
timer *g_pSlvBatchTimer;

WORD g_slv_wMstAddr;
WORD g_slv_wSlvAddr;

DWORD g_dwSlvDataSeq = 1;

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

void *slave_alloc_Login(WORD wSrcAddr, WORD wDstAddr)
{
    //log_debug(LOG1, "wPkgCount(%d).", wPkgCount);
    MSG_LOGIN_REQ_S *pstLogin = (MSG_LOGIN_REQ_S *)malloc(sizeof(MSG_LOGIN_REQ_S));
    if(pstLogin)
    {
        pstLogin->stMsgHdr.wSig = htons(START_SIG_2);
        pstLogin->stMsgHdr.wVer = htons(VERSION_INT);
        pstLogin->stMsgHdr.wSrcAddr = htons(wSrcAddr);
        pstLogin->stMsgHdr.wDstAddr = htons(wDstAddr);
        pstLogin->stMsgHdr.dwSeq = htonl(g_dwSlvDataSeq++);
        pstLogin->stMsgHdr.wCmd = htons(CMD_LOGIN);
        pstLogin->stMsgHdr.wLen = htons(0);
    }

    return (void *)pstLogin;
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

DWORD slave_registerTimerProc(void *pSlv)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    log_debug(byLogNum, "slave_registerTimerProc()");

    /* 拼接主备模块消息体 */
    MSG_LOGIN_REQ_S *pstReq = (MSG_LOGIN_REQ_S *)slave_alloc_reqMsg(CMD_LOGIN);
    if(!pstReq)
    {
        log_error(byLogNum, "alloc_slave_reqMsg error!");
        return FAILE;
    }

    dwRet = slave_send((BYTE *)pstReq, sizeof(MSG_LOGIN_REQ_S));
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "slave_send error!");
        return FAILE;
    }

    free(pstReq);

    dwRet = g_pSlvRegTimer->start(REGISTER_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "g_pSlvRegTimer->start error!");
        return FAILE;
    }
    
    return dwRet;
}

DWORD slave_batchTimerProc(void *pSlv)
{
    DWORD dwRet = SUCCESS;
    slave *pclsSlv = (slave *)pSlv;
    BYTE byLogNum = pclsSlv->byLogNum;
    
    log_debug(byLogNum, "slave batch time up!");

    dwRet = g_pSlvBatchTimer->start(NEWCFG_BATCH_FAST_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "g_pSlvBatchTimer->start error!");
        return FAILE;
    }

    if(pclsSlv->stBatch.byBatchFlag == TRUE)
    {
        DWORD dwRet = slave_batchRes2Mst(pSlv);
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "slave failed to response batch to master!");
            return FAILE;
        }
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
    MSG_LOGIN_REQ_S *pstLogin = (MSG_LOGIN_REQ_S *)slave_alloc_Login(wSlvAddr, wMstAddr);
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
    g_pSlvBatchTimer = new timer(byLogNum);
    dwRet = g_pSlvBatchTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "g_pSlvBatchTimer->init error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("slv_timer", g_pSlvBatchTimer->dwTimerFd, slave_batchTimerProc, this);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_RegTask error!");
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

