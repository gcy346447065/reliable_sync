#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
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

    return iSockFd;
}

int socket_free(int iSockFd)
{
    close(iSockFd);
    return 0;
}
