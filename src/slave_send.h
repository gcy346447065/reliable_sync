#ifndef _SLAVE_SEND_H_
#define _SLAVE_SEND_H_

#include "macro.h"

DWORD slave_send(BYTE *pbyMsg, WORD wMsgLen);
VOID *slave_alloc_reqMsg(WORD wCmd);
VOID *slave_alloc_rspMsg(WORD wSeq, WORD wCmd);

#endif//_SLAVE_SEND_H_

