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

//slave的状态
typedef struct
{
    BYTE byBatchFlag;
    BYTE bySendtimes;
    WORD wDataNums;
    vector<DWORD> vecDataIDs; //用于记录slave未收到的batch pkg
    
}SLAVE_BATCH_STATE_S;

typedef struct
{
    BYTE byInstantFlag;
    BYTE bySendtimes;
    
}SLAVE_INSTANT_STATE_S;

typedef struct
{
    BYTE byWaitedFlag;
    BYTE bySendtimes;
    
}SLAVE_WAITED_STATE_S;

typedef struct
{
    WORD wSlvAddr;
    SLAVE_BATCH_STATE_S stBatch;
    SLAVE_INSTANT_STATE_S stInstant;
    SLAVE_WAITED_STATE_S stWaited;
    
}SLAVE_S;

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
    
};

#endif//_MASTER_H_

