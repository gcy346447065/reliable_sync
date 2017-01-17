#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> //for inet_addr
#include <errno.h> //for errno
#include "socket.h"
#include "macro.h"
#include "log.h"

int socket_init(void)
{
    /* create socket */
    int iSockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(iSockFd < 0)
    {
        log_error("Create socket error!");
        return -1;
    }

    /* set socket no block */
    int iMode= 1; 
    int iCtlRet = ioctl(iSockFd, FIONBIO, &iMode);
    if(iCtlRet < 0)
    {
        log_error("Set socket no block error!");
        return -2;
    }

    struct sockaddr_in stSlaveSync2SyncAddr;
    memset(&stSlaveSync2SyncAddr, 0, sizeof(stSlaveSync2SyncAddr)); 
    stSlaveSync2SyncAddr.sin_family = AF_INET;
    stSlaveSync2SyncAddr.sin_addr.s_addr = inet_addr(SLAVE_IP); 
    stSlaveSync2SyncAddr.sin_port = htons(SLAVE_SYNC_TO_SYNC_PORT);
    if(bind(iSockFd, (struct sockaddr *)&stSlaveSync2SyncAddr, sizeof(stSlaveSync2SyncAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return -3;
    }

    struct sockaddr_in stMasterSync2SyncAddr;
    memset(&stMasterSync2SyncAddr, 0, sizeof(stMasterSync2SyncAddr)); 
    stMasterSync2SyncAddr.sin_family = AF_INET;
    stMasterSync2SyncAddr.sin_addr.s_addr = inet_addr(MASTER_IP); 
    stMasterSync2SyncAddr.sin_port = htons(MASTER_SYNC_TO_SYNC_PORT);
    if(connect(iSockFd, (struct sockaddr *)&stMasterSync2SyncAddr, sizeof(stMasterSync2SyncAddr)) < 0)
    {
        log_error("socket connect error(%d)!", errno);
        return -4;
    }

    return iSockFd;
}

int socket_free(int iSockFd)
{
    close(iSockFd);
    return 0;
}
