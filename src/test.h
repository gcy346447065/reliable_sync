#ifndef _TEST_H_
#define _TEST_H_

#include "macro.h"
#include "vos.h"
#include "mbufer.h"
#include "timer.h"

class test
{
public:
    test(BYTE byAddr = ADDR_MIN)
    {
        if(byAddr >= ADDR_MIN && byAddr <= ADDR_MAX)
        {
            byMstAddr = byAddr;
        }
        else
        {
            byMstAddr = ADDR_MIN;
        }

        byTestAddr = ADDR_10;
    }

    DWORD test_Init();
    VOID test_Free();
    VOID test_Loop();

private:
    BYTE byTestAddr;
    BYTE byMstAddr;

    vos *pVos;
    dmm *pDmm;
    mbufer *pMbufer;
    timer *pTimer;

};

#endif//_TEST_H_

