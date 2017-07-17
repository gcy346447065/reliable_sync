#ifndef _SLAVE_RECV_H_
#define _SLAVE_RECV_H_

#include "macro.h"

BYTE *slave_alloc_RecvBuffer(WORD wBufLen);
DWORD slave_free(BYTE *pRecvBuf);

DWORD slave_recv(BYTE *pbyRecvBuf, WORD *pwBufLen);
DWORD slave_msgHandle(const BYTE *pbyPara, WORD wParaLen);

#endif//_SLAVE_RECV_H_
