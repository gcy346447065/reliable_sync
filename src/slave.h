#ifndef _SLAVE_H_
#define _SLAVE_H_

#include <vector>
#include <map>
using std::vector;
using std::map;

#include "macro.h"
#include "protocol.h"
#include "vos.h"
#include "mbufer.h"
#include "log.h"
#include "timer.h"

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
        this->stBatch.byBatchFlag = FALSE;
        this->stBatch.bySendtimes = 0;
        this->stBatch.dwDataNums = 0;
        this->stBatch.dwDataStart = 0;
        this->stBatch.dwDataEnd = 0;
    }

    DWORD slave_Init();
    VOID slave_Free();
    VOID slave_Loop();

private:
    vos *pVos;
    dmm *pDmm;

public:
    BYTE byLogNum;
    WORD wTaskAddr;
    WORD wMstAddr;
    WORD wSlvAddr;
    mbufer *pMbufer;
    SLAVE_BATCH_STATE_S stBatch;

    timer *pKeepAliveTimer;
    timer *pBatchTimer;

};

#endif//_SLAVE_H_

