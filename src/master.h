#ifndef _MASTER_H_
#define _MASTER_H_

#include <vector>
#include <map>
using std::vector;
using std::map;

#include "macro.h"
#include "vos.h"
#include "mbufer.h"
#include "log.h"
#include "protocol.h"
#include "timer.h"

class master
{
public:
    master(BYTE byNum, WORD wAddr)
    {
        if(wAddr >= ADDR_MIN && wAddr <= ADDR_MAX)
        {
            wMstAddr = wAddr;
        }
        else
        {
            log_error(byNum, "master addr error!");
            wMstAddr = ADDR_MIN;
        }

        byLogNum = byNum;
        wTaskAddr = ADDR_MstTask;//在START_SIG_1的login消息中记录
    }

    DWORD master_Init();
    VOID master_Free();
    VOID master_Loop();

private:
    vos *pVos;
    dmm *pDmm;
    
public:
    WORD wMstAddr;
    WORD wTaskAddr;
    BYTE byLogNum;
    mbufer *pMbufer;
    
    BYTE bySlvNums;
    vector<SLAVE_S> vecSlvs;

    BYTE byBatchFlag; // 当前master是否正处于Batch状态，TRUE-是，FALSE-否
    BYTE byInstantFlag;
    BYTE byWaitedFlag;
    map<DWORD, NODE_DATA_BATCH_S*> mapDataBatch; // master用于记录已存储的batch pkg
    map<DWORD, NODE_DATA_INSTANT_S*> mapDataInstant;
    map<DWORD, NODE_DATA_WAITED_S*> mapDataWaited;

    timer *pKeepAliveTimer;
    timer *pBatchTimer;
    timer *pWaitedTimer; // 用于判断是否满足waited定时阈值
    DWORD dwWaitedSize; // 用于计算是否满足waited定量阈值
    
};

#endif//_MASTER_H_

