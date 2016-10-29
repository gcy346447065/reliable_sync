#ifndef _MACRO_H_
#define _MACRO_H_

#define MAX_EPOLL_NUM 1024


////////////////////////////
#define SERVER_IP "192.168.11.114"
#define SERVER_PORT 8765
#define BUFFER_SIZE 1024

#define FILE_10M_ADDR "/home/guochengying/sync/syncServer/file_10M"
#define FILE_10M_LENGTH 11002748

enum SERVER_SOCKET_STATUS
{
    SERVER_SOCKET_WAIT_SYN,
    SERVER_SOCKET_SEND_SYN_ACK,
    SERVER_SOCKET_READY
};

enum CLIENT_SOCKET_STATUS
{
    CLIENT_SOCKET_SEND_SYN,
    CLIENT_SOCKET_READY
};

enum BACKUP_STATUS
{
    BACKUP_NULL,
    BACKUP_BATCH,
    BACKUP_REALTIME_WAITING,
    BACKUP_REALTIME_INSTANT
};


#endif //_MACRO_H_