#ifndef _SLAVE_SEND_H_
#define _SLAVE_SEND_H_

#include "macro.h"

DWORD slave_send(BYTE *pbyMsg, WORD wMsgLen);
VOID *slave_alloc_reqMsg(BYTE byCmd);

#endif//_SLAVE_SEND_H_
