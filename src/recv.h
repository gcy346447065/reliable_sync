#ifndef _NEW_CFG_H_
#define _NEW_CFG_H_

int recvFromMasterSync(int iSyncSockFd, void *pMsg, int iMaxMsgLen);
int handle_sync_msg(const char *pcMsg, int iMsgLen);

#endif //_NEW_CFG_H_