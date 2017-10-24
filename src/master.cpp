#include <unistd.h> //for read STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <stdio.h> //for sscanf
#include <errno.h> //for errno
#include "master.h"
#include "macro.h"
#include "log.h"


DWORD master_stdinProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("test_stdinProc()");

    //从控制台读取键入字符串
    CHAR *pcStdinBuf = (CHAR *)malloc(MAX_STDIN_FILE_LEN);
    memset(pcStdinBuf, 0, MAX_STDIN_FILE_LEN);
    INT iRet = read(STDIN_FILENO, pcStdinBuf, MAX_STDIN_FILE_LEN);
    if(iRet <= 0)
    {
        log_error("read STDIN_FILENO error(%s)!", strerror(errno));
        return FAILE;
    }
    pcStdinBuf[iRet - 1] = '\0'; //-1 for '\n' turn to '\0'
    //log_debug("pcStdinBuf(%s)", pcStdinBuf);

    CHAR acWord1[16], acWord2[16];
    memset(acWord1, 0, 16);
    memset(acWord2, 0, 16);
    if(sscanf(pcStdinBuf, "get %[^ ] %[^ ]", acWord1, acWord2) == 2)
    {
        //log_debug("acWord1(%s), acWord2(%s)", acWord1, acWord2);
        if(strcmp(acWord1, "batch") == 0)
        {
            if(strcmp(acWord2, "count") == 0)
            {
                //log_debug("data_getBatchCount(%lu)", g_mst_pDataList->data_getBatchCount());
            }
            else if(strcmp(acWord2, "new") == 0)
            {
                //log_debug("data_getBatchNew(%lu)", g_mst_pDataList->data_getBatchNew());
            }
        }
        else if(strcmp(acWord1, "instant") == 0)
        {
            if(strcmp(acWord2, "count") == 0)
            {
                //log_debug("data_getInstantCount(%lu)", g_mst_pDataList->data_getInstantCount());
            }
            else if(strcmp(acWord2, "new") == 0)
            {
                //log_debug("data_getInstantNew(%lu)", g_mst_pDataList->data_getInstantNew());
            }
        }
        else if(strcmp(acWord1, "waited") == 0)
        {
            if(strcmp(acWord2, "count") == 0)
            {
                //log_debug("data_getWaitedCount(%lu)", g_mst_pDataList->data_getWaitedCount());
            }
            else if(strcmp(acWord2, "new") == 0)
            {
                //log_debug("data_getWaitedNew(%lu)", g_mst_pDataList->data_getWaitedNew());
            }
        }
    }
    else if(sscanf(pcStdinBuf, "print %[^ ]", acWord1) == 1)
    {
        if(strcmp(acWord1, "batch") == 0)
        {
            //g_mst_pDataList->data_traverseAndPrintBatch();
        }
        else if(strcmp(acWord1, "instant") == 0)
        {
            //g_mst_pDataList->data_traverseAndPrintInstant();
        }
        else if(strcmp(acWord1, "waited") == 0)
        {
            //g_mst_pDataList->data_traverseAndPrintWaited();
        }
    }

    return dwRet;
}

DWORD master_mailboxProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    //log_debug("master_mailboxProc()");

    /*BYTE *pbyRecvBuf = master_alloc_RecvBuffer(MAX_RECV_LEN);
    if(pbyRecvBuf == NULL)
    {
        log_error("master_alloc_RecvBuffer error!");
        return FAILE;
    }

    WORD wBufLen = MAX_RECV_LEN;
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
    }*/

    return dwRet;
}

DWORD master_keepaliveTimerProc(void *pObj)
{
    DWORD dwRet = SUCCESS;
    log_debug("master_keepaliveTimerProc()");
    //log_debug("slv_getSlvNum(%d)", g_pMstMbufer->g_mst_pSlvList->slv_getSlvNum());

    /*MSG_KEEP_ALIVE_REQ_S *pstReq = (MSG_KEEP_ALIVE_REQ_S *)master_alloc_reqMsg(0, CMD_KEEP_ALIVE);//！这里没有写入备机地址以方便复用
    if(!pstReq)
    {
        log_error("master_alloc_reqMsg error!");
        return FAILE;
    }

    //查看备机注册表中各备机的保活发送次数，如大于3次则解注册该备机，否则发送保活包
    BYTE *pbyRetSlvAddrs = (BYTE *)malloc(sizeof(BYTE) * g_mst_pSlvList->slv_getSlvNum());
    dwRet = g_mst_pSlvList->slv_traverseAndRetSlvAddr(pbyRetSlvAddrs);//slv_getSlvNum该值会在其中更新
    if(dwRet != SUCCESS)
    {
        log_error("slv_traverseAndRetSlvAddr error!");
        free(pstReq);
        return FAILE;
    }

    //log_debug("slv_getSlvNum(%d)", g_pMstMbufer->g_mst_pSlvList->slv_getSlvNum());
    for(UINT i = 0; i < g_mst_pSlvList->slv_getSlvNum(); i++)
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
    }*/
    
    return dwRet;
}

DWORD master::master_Init()
{
    log_init("MASTER");
    log_debug("Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    vecSlvAddr.clear();
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
    dwRet = pDmm->create_mailbox(&pMbufer, byMstAddr, "mst_mb");
    if(dwRet != SUCCESS)
    {
        log_error("create_mailbox error!");
        return FAILE;
    }
    dwRet = pVos->vos_RegTask("mst_mb", pMbufer->dwSocketFd, master_mailboxProc, NULL);
    if(dwRet != SUCCESS)
    {
        log_error("vos_RegTask error!");
        return FAILE;
    }

    /* 向vos注册stdin事件 */
    dwRet = pVos->vos_RegTask("mst_stdin", STDIN_FILENO, master_stdinProc, NULL);
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

