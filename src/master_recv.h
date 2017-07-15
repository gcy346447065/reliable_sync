#ifndef _MASTER_RECV_H_
#define _MASTER_RECV_H_

#include "macro.h"

BYTE *master_alloc_RecvBuffer(WORD wBufLen);
DWORD master_free(BYTE *pRecvBuf);

DWORD master_recv(BYTE *pbyRecvBuf, WORD *pwBufLen);
DWORD master_msgHandle(const BYTE *pbyPara, WORD wParaLen);

#endif//_MASTER_RECV_H_
