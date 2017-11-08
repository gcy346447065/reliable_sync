#ifndef _MASTER_SEND_H_
#define _MASTER_SEND_H_

#include "macro.h"


VOID *master_alloc_reqMsg(WORD wSrcAddr, WORD wDstAddr, WORD wCmd);
VOID *master_alloc_rspMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, DWORD dwSeq, WORD wCmd);

DWORD master_sendToTask(void *pMst, void *pData, WORD wDataLen);
DWORD master_sendToSlaves(void *pMst, void *pData, WORD wDataLen);


#endif//_MASTER_SEND_H_

