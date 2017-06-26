#ifndef _SYNC_MASTER_H_
#define _SYNC_MASTER_H_

#include "macro.h"
#include "event.h"

/*
 * 发送端，但可能发送到多个地址
 * 每需要一条同步通道则实例化一次
 */
class sync_master
{

public:
    CHAR g_cMasterStatus;

    DWORD init(DWORD dwLclAddr, DWORD dwOppAddr, DWORD dwTimeThr, DWORD dwQttyThr);
    DWORD sendInstant(void *pBuf, DWORD dwBufLen, DWORD dwOrderRange, DWORD dwOrderNum);
    DWORD sendWaited(void *pBuf, DWORD dwBufLen, DWORD dwOrderRange, DWORD dwOrderNum);

private:
    DWORD dwNewcfgID;

};

#endif //_SYNC_MASTER_H_
