#ifndef _SEND_H_
#define _SEND_H_

#include "protocol.h"

int sendToMasterSync(int iSyncSockFd, const void *pMsg, int iMsgLen);

MSG_HEADER *alloc_slave_reqMsg(char cCmd);
MSG_HEADER *alloc_slave_rspMsg(char cCmd, char cSeq);
MSG_NEWCFG_WAITED_RSP *alloc_slave_newCfgWaitedRsp(char cSeq, unsigned int uiMsgLen);

#endif //_SEND_H_