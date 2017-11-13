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
    slave(BYTE byNum, BYTE wMstAddr, BYTE wSlvAddr)
    {
        if(wMstAddr >= ADDR_MIN && wMstAddr <= ADDR_MAX && wSlvAddr >= ADDR_MIN && wSlvAddr <= ADDR_MAX)
        {
            this->wMstAddr = wMstAddr;
            this->wSlvAddr = wSlvAddr;
        }
        else
        {
            log_error(byNum, "slave addr error!");
            this->wMstAddr = ADDR_MIN;
            this->wSlvAddr = ADDR_MIN;
        }

        this->byLogNum = byNum;
    }

    DWORD slave_Init();
    VOID slave_Free();
    VOID slave_Loop();

private:
    vos *pVos;
    dmm *pDmm;

public:
    BYTE byLogNum;
    WORD wMstAddr;
    WORD wSlvAddr;
    mbufer *pMbufer;
    map<WORD, DATA_BATCH_PKG_S> mapDataBatch;
    map<WORD, DATA_PKG_S> mapDataInstant;
    map<WORD, DATA_PKG_S> mapDataWaited;

};

#endif//_SLAVE_H_

