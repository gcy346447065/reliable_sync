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

class master
{
public:
    master(BYTE byAddr = 1)
    {
        if(byAddr >= ADDR_MIN && byAddr <= ADDR_MAX)
        {
            byMstAddr = byAddr;
        }
        else
        {
            byMstAddr = ADDR_MIN;
        }
    }

    DWORD master_Init();
    VOID master_Free();
    VOID master_Loop();

    /*DWORD master_stdinProc(void *pObj);
    DWORD master_mailboxProc(void *pObj);
    DWORD master_keepaliveTimerProc(void *pObj);*/

private:
    BYTE byMstAddr;
    vector<BYTE> vecSlvAddr;
    map<WORD, MSG_DATA_S> mapBatchData;
    map<WORD, MSG_DATA_S> mapInstantData;
    map<WORD, MSG_DATA_S> mapWaitedData;

    vos *pVos;
    dmm *pDmm;
    mbufer *pMbufer;

    
    
};

#endif//_MASTER_H_

