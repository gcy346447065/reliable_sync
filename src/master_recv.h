#ifndef _MASTER_RECV_H_
#define _MASTER_RECV_H_

#include "macro.h"

void *master_allocRecvBuffer(WORD wBufLen);
DWORD master_freeRecvBuffer(void *pRecvBuf);

DWORD master_recv(void *pMst, void *pRecvBuf, WORD *pwBufLen);
DWORD master_msgHandle(void *pMst, const void *pMsg, WORD wMsgLen);


#endif//_MASTER_RECV_H_

