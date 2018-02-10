#ifndef _SLAVE_SEND_H_
#define _SLAVE_SEND_H_

#include "macro.h"

DWORD slave_send(BYTE *pbyMsg, WORD wMsgLen);
VOID *slave_alloc_reqMsg(WORD wCmd);
VOID *slave_alloc_rspMsg(WORD wSrcAddr, WORD wDstAddr, WORD wSig, DWORD dwSeq, WORD wCmd);
VOID *slave_alloc_batchRspMsg(WORD wSrcAddr, WORD wDstAddr, DWORD dwSeq, DWORD dwDataNum, DWORD dwDataStart, DWORD dwDataEnd);

DWORD slave_sendMsg(void *pSlv, WORD wDstAddr, void *pData, WORD wDataLen);


#endif//_SLAVE_SEND_H_

