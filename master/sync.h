#ifndef _SYNC_H_
#define _SYNC_H_

enum SEND_METHOD_TO_SYNC
{
    SEND_BATCH,
    SEND_REALTIME_WAITED,
    SEND_REALTIME_INSTANT
};

int SendToSync(void *pBuf, int iBufLen, int iMaxPkgLen, void *pDestAddr, int iSendMethod);
int RecvFromSync(void *pBuf, int iBufLen);


#endif //_SYNC_H_