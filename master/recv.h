#ifndef _RECV_H_
#define _RECV_H_

int handle_sync_msg(const char *pcMsg, int iMsgLen);

int recvFromSlaveSync(int iSyncSockFd, void *pMsg, int iMaxMsgLen);

#endif //_RECV_H_
