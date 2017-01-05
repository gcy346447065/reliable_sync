#ifndef _SEND_H_
#define _SEND_H_

#include "protocol.h"

int send2SlaveSync(int iSyncSockFd, const void *pMsg, int iMsgLen);

MSG_HEADER *alloc_master_rspMsg(char cCmd, char cSeq);
MSG_HEADER *alloc_master_reqMsg(char cCmd);

MSG_NEWCFG_INSTANT_REQ *alloc_master_newCfgInstantReq(void *pData, int iDataLen, int iNewCfgID);
MSG_NEWCFG_WAITED_REQ *alloc_master_newCfgWaitedReq(int iMsgLen);

#endif //_SEND_H_
