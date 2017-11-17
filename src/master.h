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
        wTaskAddr = 0;//在START_SIG_1的login消息中记录
    }

    DWORD master_Init();
    VOID master_Free();
    VOID master_Loop();

private:
    vos *pVos;
    dmm *pDmm;
    
public:
    BYTE byLogNum;
    WORD wMstAddr;
    WORD wTaskAddr;
    vector<WORD> vecSlvAddr;
    map<DWORD, void*> mapDataBatch;//NODE_DATA_BATCH_S*
    map<DWORD, void*> mapDataInstant;//NODE_DATA_INSTANT_S*
    map<DWORD, void*> mapDataWaited;//NODE_DATA_WAITED_S*
    BYTE byBatchFlag;
    DWORD dwBatchNow;
    BYTE byInstantFlag;
    DWORD dwInstantNow;
    BYTE byWaitedFlag;
    DWORD dwWaitedNow;
    
    mbufer *pMbufer;
    
};

#endif//_MASTER_H_

