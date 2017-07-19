#ifndef _MASTER_SEND_H_
#define _MASTER_SEND_H_

#include "macro.h"

DWORD master_sendToOne(BYTE bySlvAddr, BYTE *pbyMsg, WORD wMsgLen);
DWORD master_sendToMany(BYTE abySlvMsgAddrs[], BYTE *pbyMsg, WORD wMsgLen);

VOID *master_alloc_reqMsg(BYTE bySlvAddr, BYTE byCmd);
VOID *master_alloc_rspMsg(BYTE bySlvAddr, BYTE bySeq, BYTE byCmd);

#endif//_MASTER_SEND_H_
