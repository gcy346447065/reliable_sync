#ifndef _SLAVE_RECV_H_
#define _SLAVE_RECV_H_

#include "macro.h"

DWORD slave_recv(void *pSlv, void *pRecvBuf, WORD *pwBufLen);
DWORD slave_msgHandle(void *pSlv, const void *pMsg, WORD wMsgLen);

#endif//_SLAVE_RECV_H_

