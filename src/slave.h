#ifndef _SLAVE_H_
#define _SLAVE_H_

#include <map>
using std::map;

#include "macro.h"
#include "protocol.h"
#include "vos.h"
#include "mbufer.h"

class slave
{
public:
    slave(BYTE byMstAddr = ADDR_MIN, BYTE bySlvAddr = ADDR_2)
    {
        if(byMstAddr >= ADDR_MIN && byMstAddr <= ADDR_MAX)
        {
            this->byMstAddr = byMstAddr;
            this->bySlvAddr = bySlvAddr;
        }
        else
        {
            this->byMstAddr = ADDR_MIN;
            this->bySlvAddr = ADDR_2;
        }
    }

    DWORD slave_Init();
    VOID slave_Free();
    VOID slave_Loop();

private:
    vos *pVos;
    dmm *pDmm;

public:
    mbufer *pMbufer;

    BYTE byMstAddr;
    BYTE bySlvAddr;
    map<WORD, DATA_BATCH_PKG_S> mapDataBatch;
    map<WORD, DATA_PKG_S> mapDataInstant;
    map<WORD, DATA_PKG_S> mapDataWaited;

};

#endif//_SLAVE_H_

