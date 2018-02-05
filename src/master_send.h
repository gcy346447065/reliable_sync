#ifndef _MASTER_SEND_H_
#define _MASTER_SEND_H_

#include "macro.h"


VOID *master_alloc_reqMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, WORD wCmd);
VOID *master_alloc_rspMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, DWORD dwSeq, WORD wCmd);
void *master_alloc_dataBatch(WORD wSrcAddr, WORD wDstAddr, DWORD dwDataStart, DWORD dwDataEnd, 
                                    DWORD dwDataID, void *pBuf, WORD wBufLen);
void *master_alloc_dataInstant(WORD wSrcAddr, WORD wDstAddr, DWORD dwDataID, void *pBuf, WORD wBufLen);

DWORD master_sendMsg(void *pMst, WORD wDstAddr, void *pData, WORD wDataLen);
DWORD master_sendToSlaves(void *pMst, void *pData, WORD wDataLen);


#endif//_MASTER_SEND_H_

