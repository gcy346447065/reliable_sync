#ifndef _MASTER_H_
#define _MASTER_H_

#include <vector>
#include <map>
using std::vector;
using std::map;

#include "macro.h"
#include "protocol.h"
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
    map<WORD, DATA_BATCH_PKG_S> mapDataBatch;
    map<WORD, DATA_PKG_S> mapDataInstant;
    map<WORD, DATA_PKG_S> mapDataWaited;
    
    mbufer *pMbufer;
    
};

#endif//_MASTER_H_

