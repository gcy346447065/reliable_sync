#ifndef _MASTER_SEND_H_
#define _MASTER_SEND_H_

#include "macro.h"


VOID *master_alloc_reqMsg(BYTE byMstAddr, BYTE bySlvAddr, WORD wCmd);
VOID *master_alloc_rspMsg(BYTE byMstAddr, BYTE bySlvAddr, WORD dwSeq, WORD wCmd);

#endif//_MASTER_SEND_H_

