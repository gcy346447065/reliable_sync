#include <unistd.h> //for read STDIN_FILENO
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <stdio.h> //for sscanf
#include <errno.h> //for errno
#include <pthread.h> //for pthread
#include <netinet/in.h> //for htons
#include <iostream> //for cout
#include "master.h"
#include "master_send.h"
#include "macro.h"
#include "protocol.h"
#include "log.h"
#include "master_recv.h"
#include "master_send.h"
#include "checksum.h"
#include "timer.h"

using namespace std;

typedef struct
{
    master *pclsMst;
}MASTER_PROCTHREAD_S;

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

DWORD master_keepaliveTimerProc(void *pMst)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    log_debug(byLogNum, "master_keepaliveTimerProc().");

    //判断slave的保活情况
    SLAVE_S *pstSlv;
    UINT uiSlvIdx;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        pstSlv = (SLAVE_S *)&pclsMst->vecSlvs.at(uiSlvIdx);
        
        pstSlv->wKeepAliveCnt++;
        if(pstSlv->wKeepAliveCnt > MAX_KEEPALIVE_TIMES)
        {
            pclsMst->vecSlvs.erase(pclsMst->vecSlvs.begin() + uiSlvIdx); 
            pclsMst->bySlvNums--;
            
            log_debug(byLogNum, "Slave(%u) has lost! pclsMst->bySlvNums = %u.", pstSlv->wSlvAddr, pclsMst->bySlvNums);
            continue;
        }

        //向slave发送自己的KeepAlive心跳报文
        MSG_KEEP_ALIVE_REQ_S *pstKeepAlivePkg = (MSG_KEEP_ALIVE_REQ_S *)master_alloc_reqMsg(pclsMst->wMstAddr, pstSlv->wSlvAddr, START_SIG_2, CMD_KEEP_ALIVE);

        master_sendMsg((void *)pclsMst, pstSlv->wSlvAddr, (void *)pstKeepAlivePkg, sizeof(MSG_KEEP_ALIVE_REQ_S));
    }
    
    dwRet = pclsMst->pKeepAliveTimer->start(KEEPALIVE_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pWaitedTimer->start error!");
        return FAILE;
    }
    
    return dwRet;
}

DWORD master_waitedTimerProc(void *pMst)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;

    log_debug(byLogNum, "master_waitedTimerProc()");

    SLAVE_S *pstSlv;
    UINT uiSlvIdx;
    MSG_DATA_WAITED_REQ_S *pstWaitedPkg;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        pstSlv = (SLAVE_S *)&pclsMst->vecSlvs.at(uiSlvIdx);
 
        for(map<DWORD, NODE_DATA_WAITED_S*>::iterator it = pclsMst->mapDataWaited.begin(); it != pclsMst->mapDataWaited.end(); it++) {
            DWORD dwWaitedID = it->first;
            NODE_DATA_WAITED_S* pstWaited = it->second; //从map中取出节点数据

            if (pstWaited->wSendTimes >= MAX_SEND_TIMES) {
                // 节点重发次数过多，删除节点
                free(pstWaited);
                pclsMst->mapDataWaited.erase(it);
                continue;
            }
                    
            void *pData = (void *)pstWaited->stWaitedNet.abyData;
            WORD wDataLen = pstWaited->stWaitedNet.wDataLen;

            pstWaitedPkg = (MSG_DATA_WAITED_REQ_S *)master_alloc_dataWaited(pclsMst->wMstAddr, pstSlv->wSlvAddr, dwWaitedID, pData, wDataLen);
            pstWaitedPkg->stData.wDataChecksum = htons(checksum((const void *)pstWaitedPkg->stData.abyData, wDataLen));
            
            log_hex(byLogNum, (void *)pstWaitedPkg, 30);
            log_debug(byLogNum, "master_waitedTimerProc master_sendMsg");
            master_sendMsg((void *)pclsMst, pstSlv->wSlvAddr, (void *)pstWaitedPkg, sizeof(MSG_DATA_WAITED_REQ_S) + wDataLen);

            pstWaited->wSendTimes++;
            free(pstWaitedPkg);
        }
    }

    dwRet = pclsMst->pWaitedTimer->start(WAITED_TIMER_VALUE);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pWaitedTimer->start error!");
        return FAILE;
    }
    
    return dwRet;
}

DWORD master_batchTimerProc(void *pMst)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pMst;
    BYTE byLogNum = pclsMst->byLogNum;
    
    log_debug(byLogNum, "master batch time up!");

    SLAVE_S *pstSlv;
    unsigned int uiSlvIdx;
    DWORD dwBatchFinNums = 0;
    DWORD dwSlaveNums = 0;
    for(uiSlvIdx = 0; uiSlvIdx < pclsMst->vecSlvs.size(); uiSlvIdx++)
    {
        dwSlaveNums++;
        pstSlv = (SLAVE_S *)&(pclsMst->vecSlvs.at(uiSlvIdx));
        if(pstSlv->stBatch.byBatchFlag == TRUE)
        {
            if((pstSlv->stBatch.bySendtimes++) > MAX_RETRANS_TIMES)
            {
                log_debug(byLogNum, "Slave(%u)'s retransmit max!", pstSlv->wSlvAddr);
                pstSlv->stBatch.byBatchFlag = FALSE;
                pstSlv->stBatch.bySendtimes = 0;
                pstSlv->stBatch.dwDataNums = 0;
                pstSlv->stBatch.vecDataIDs.clear();
                
                dwBatchFinNums++;
            }
            
            for(DWORD i = 0; i < pstSlv->stBatch.dwDataNums; i++)
            {
                DWORD dwDataID = pstSlv->stBatch.vecDataIDs.at(i);
                NODE_DATA_BATCH_S *pstNodeBatch = pclsMst->mapDataBatch[dwDataID];
                if(!pstNodeBatch)
                {
                    log_error(byLogNum, "master failed to get batch node!");
                    continue;
                }
                
                MSG_DATA_BATCH_REQ_S *pstBatchPkg = (MSG_DATA_BATCH_REQ_S *)master_alloc_dataBatch(pclsMst->wMstAddr, pstSlv->wSlvAddr, pstNodeBatch->stBatchNet.dwDataStart, pstNodeBatch->stBatchNet.dwDataEnd,
                                                                  dwDataID, (void *)pstNodeBatch->stBatchNet.stData.abyData, pstNodeBatch->stBatchNet.stData.wDataLen);

                master_sendMsg((void *)pclsMst, pstSlv->wSlvAddr, (void *)pstBatchPkg, sizeof(MSG_DATA_BATCH_REQ_S) + pstNodeBatch->stBatchNet.stData.wDataLen);

                free(pstBatchPkg);
            }
        }
        else
        {
            dwBatchFinNums++;
        }
    }

    if(dwBatchFinNums < dwSlaveNums)
    {
        //依然有slave处于batch状态
        log_debug(byLogNum, "%u slave(s) still need retransmit!", dwSlaveNums - dwBatchFinNums);
        
        dwRet = pclsMst->pBatchTimer->start(NEWCFG_BATCH_FAST_TIMER_VALUE); //快速重传定时3s
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "pclsMst->pBatchTimer start error!");
            return FAILE;
        }
    }
    else
    {
        //所有slave都batch完毕
        log_debug(byLogNum, "Data batch finished!");
        
        cout << "Batch finished." << endl;
        
        pclsMst->byBatchFlag = FALSE;
        pclsMst->mapDataBatch.clear();
        
        dwRet = pclsMst->pBatchTimer->stop();
        if(dwRet != SUCCESS)
        {
            log_error(byLogNum, "pclsMst->pBatchTimer stop error!");
            return FAILE;
        }
    }

    return SUCCESS;
}

DWORD master_initTimer(void *pArg)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pArg;
    BYTE byLogNum = pclsMst->byLogNum;

    //KeepAlive定时器
    pclsMst->pKeepAliveTimer = new timer(byLogNum);
    dwRet = pclsMst->pKeepAliveTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsMst->pKeepAliveTimer init error!");
        return FAILE;
    }

    //Batch定时器
    pclsMst->pBatchTimer = new timer(byLogNum);
    dwRet = pclsMst->pBatchTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsMst->pBatchTimer init error!");
        return FAILE;
    }

    //Waited定时器
    pclsMst->pWaitedTimer = new timer(byLogNum);
    dwRet = pclsMst->pWaitedTimer->init();
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsMst->pWaitedTimer init error!");
        return FAILE;
    }

    return SUCCESS;
}

DWORD master_startTimer(void *pArg)
{
    DWORD dwRet = SUCCESS;
    master *pclsMst = (master *)pArg;
    BYTE byLogNum = pclsMst->byLogNum;

    dwRet = pclsMst->pKeepAliveTimer->start(KEEPALIVE_TIMER_VALUE); // 1min -> 6s
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsMst->pKeepAliveTimer start error!");
        return FAILE;
    }

    dwRet = pclsMst->pWaitedTimer->start(WAITED_TIMER_VALUE); // 1min -> 6s
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "pclsMst->pWaitedTimer start error!");
        return FAILE;
    }

    return SUCCESS;
}

DWORD master::master_Init()
{
    log_init(byLogNum, "");
    log_debug(byLogNum, "Master Task Beginning.");
    DWORD dwRet = SUCCESS;

    //初始化master的slave列表
    bySlvNums = 0;
    vecSlvs.clear();

    //初始化backup状态
    mapDataBatch.clear();
    mapDataInstant.clear();
    mapDataWaited.clear();
    byBatchFlag = FALSE;
    byInstantFlag = FALSE;
    byWaitedFlag = FALSE;
    
    pVos = new vos(byLogNum);
    dwRet = pVos->vos_Init();//实际为创建epoll
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "vos_Init error!");
        return FAILE;
    }
    pDmm = new dmm(byLogNum);
    pMbufer = new mbufer(byLogNum);

    /* 创建邮箱并注册到vos  */
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

    //master的定时器
    dwRet |= master_initTimer(this);

    dwRet |= pVos->vos_RegTask("mst_waited_timer", pWaitedTimer->dwTimerFd, master_waitedTimerProc, this);
    dwRet |= pVos->vos_RegTask("mst_ka_timer", pKeepAliveTimer->dwTimerFd, master_keepaliveTimerProc, this);
    dwRet |= pVos->vos_RegTask("mst_batch_timer", pBatchTimer->dwTimerFd, master_batchTimerProc, this);

    dwRet |= master_startTimer(this);
    if(dwRet != SUCCESS)
    {
        log_error(byLogNum, "master set timer error!");
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

