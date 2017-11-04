#ifndef _SLAVE_H_
#define _SLAVE_H_

#include <map>
using std::map;

#include "macro.h"
#include "protocol.h"
#include "vos.h"
#include "mbufer.h"
#include "log.h"

class slave
{
public:
    slave(BYTE byMstAddr = ADDR_MIN, BYTE bySlvAddr = ADDR_2, BYTE byNum = LOG1)
    {
        if(byMstAddr >= ADDR_MIN && byMstAddr <= ADDR_MAX)
        {
            this->byMstAddr = byMstAddr;
            this->bySlvAddr = bySlvAddr;
            byLogNum = byNum;
        }
        else
        {
            this->byMstAddr = ADDR_MIN;
            this->bySlvAddr = ADDR_2;
            byLogNum = byNum;
        }
    }

    DWORD slave_Init();
    VOID slave_Free();
    VOID slave_Loop();

private:
    vos *pVos;
    dmm *pDmm;

public:
    BYTE byLogNum;
    BYTE byMstAddr;
    BYTE bySlvAddr;
    mbufer *pMbufer;
    map<WORD, DATA_BATCH_PKG_S> mapDataBatch;
    map<WORD, DATA_PKG_S> mapDataInstant;
    map<WORD, DATA_PKG_S> mapDataWaited;

};

#endif//_SLAVE_H_

