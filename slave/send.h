#ifndef _SEND_H_
#define _SEND_H_

#include "protocol.h"

int send2MasterSync(int iSyncSockFd, const void *pMsg, int iMsgLen);

MSG_HEADER *alloc_slave_reqMsg(char cCmd, int iLength);
MSG_HEADER *alloc_slave_rspMsg(char cCmd, char cSeq);

#endif //_SEND_H_