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

typedef struct
{
    WORD wSlvAddr;
    BYTE byBatchFlag;
    BYTE byInstantFlag;
    BYTE byWaitedFlag;
    DWORD dwBatchNow;
    DWORD dwInstantNow;
    DWORD dwWaitedNow;
    BYTE bySendTimes;
    
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
    BYTE byLogNum;
    WORD wTaskAddr;
    WORD wMstAddr;
    vector<SLAVE_S> vecSlvs;
    map<DWORD, void*> mapDataBatch;//NODE_DATA_BATCH_S*
    map<DWORD, void*> mapDataInstant;//NODE_DATA_INSTANT_S*
    map<DWORD, void*> mapDataWaited;//NODE_DATA_WAITED_S*
    mbufer *pMbufer;
    
};

#endif//_MASTER_H_

