#ifndef _SEND_H_
#define _SEND_H_

#include "protocol.h"

int send2SlaveSync(int iSyncSockFd, const void *pMsg, int iMsgLen);

int alloc_master_rspMsg(char cmd, char seq, void **ppMsg);
int alloc_master_reqMsg(char cmd, void **ppMsg);
int alloc_master_newCfgReq(void *pData, int iDataLen, void **ppMsg, int *piMsgLen);

#endif //_SEND_H_