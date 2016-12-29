#ifndef _MACRO_H_
#define _MACRO_H_

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_NEWCFG = 2
};

#define NEW_CFG_STR ":"
#define NEW_CFG_STR_LEN strlen(NEW_CFG_STR)

#define MAX_EPOLL_NUM 1024
#define NEW_CFG_LEN 100
#define MAX_PKG_LEN 1024
#define MAX_BUFFER_SIZE 1024

#define MASTER_IP "192.168.11.114"
#define MASTER_SYNC_TO_SYNC_PORT   8761

#define SLAVE_IP "192.168.11.114"
#define SLAVE_SYNC_TO_SYNC_PORT    8762

#define LOGIN_TIMER_VALUE           (1000*10)
#define KEEPALIVE_TIMER_VALUE       (1000*60*3)
#define CHECKALIVE_TIMER_VALUE      (1000*60*10)


#endif //_MACRO_H_
