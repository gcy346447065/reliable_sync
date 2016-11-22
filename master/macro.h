#ifndef _MACRO_H_
#define _MACRO_H_

#define MAX_EPOLL_NUM 1024

#define MASTER_IP "192.168.11.114"
#define MASTER_MAIN_PORT            8766
#define MASTER_SYNC_PORT            8765
#define MASTER_SYNC_FOR_SYNC_PORT   8764

#define SLAVE_IP "192.168.11.114"
#define SLAVE_MAIN_PORT             8763
#define SLAVE_SYNC_PORT             8762
#define SLAVE_SYNC_FOR_SYNC_PORT    8761






//////////////////////////// unused
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
//////////////////////////// unused

#endif //_MACRO_H_