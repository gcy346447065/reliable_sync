#ifndef _SLAVE_SEND_H_
#define _SLAVE_SEND_H_

#include "macro.h"

DWORD slave_send(BYTE *pbyMsg, WORD wMsgLen);
VOID *slave_alloc_reqMsg(BYTE byCmd);
VOID *slave_alloc_rspMsg(BYTE bySeq, BYTE byCmd);

#endif//_SLAVE_SEND_H_
