#ifndef _SLAVE_RECV_H_
#define _SLAVE_RECV_H_

#include "macro.h"

DWORD slave_recv(void *pSlv, BYTE *pbyRecvBuf, WORD *pwBufLen);
DWORD slave_msgHandle(void *pSlv, const BYTE *pbyPara, WORD wParaLen);

#endif//_SLAVE_RECV_H_

