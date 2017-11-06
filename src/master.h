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
    master(BYTE byAddr = ADDR_MIN, BYTE byNum = LOG1)
    {
        if(byAddr >= ADDR_MIN && byAddr <= ADDR_MAX)
        {
            byMstAddr = byAddr;
            byLogNum = byNum;
        }
        else
        {
            byMstAddr = ADDR_MIN;
            byLogNum = byNum;
        }
    }

    DWORD master_Init();
    VOID master_Free();
    VOID master_Loop();

private:
    vos *pVos;
    dmm *pDmm;
    
public:
    BYTE byLogNum;
    BYTE byMstAddr;
    vector<BYTE> vecSlvAddr;
    map<WORD, DATA_BATCH_PKG_S> mapDataBatch;
    map<WORD, DATA_PKG_S> mapDataInstant;
    map<WORD, DATA_PKG_S> mapDataWaited;
    
    mbufer *pMbufer;
    
};

#endif//_MASTER_H_

