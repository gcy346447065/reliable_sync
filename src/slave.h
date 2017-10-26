#ifndef _SLAVE_H_
#define _SLAVE_H_

#include <map>
using std::map;

#include "macro.h"
#include "protocol.h"
#include "vos.h"
#include "mbufer.h"

DWORD slave_InitAndLoop(BYTE byMasterAddr, BYTE bySlaveAddr);

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

    /*DWORD slave_stdinProc(void *pObj);
    DWORD slave_mailboxProc(void *pObj);
    DWORD slave_keepaliveTimerProc(void *pObj);*/

private:
    BYTE byMstAddr;
    BYTE bySlvAddr;
    map<WORD, MSG_DATA_S> mapBatchData;
    map<WORD, MSG_DATA_S> mapInstantData;
    map<WORD, MSG_DATA_S> mapWaitedData;

    vos *pVos;
    dmm *pDmm;
    mbufer *pMbufer;

    
    
};

#endif//_SLAVE_H_

