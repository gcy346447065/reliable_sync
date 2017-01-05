#ifndef _MACRO_H_
#define _MACRO_H_

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_NEWCFG = 2
};

#define MAX_EPOLL_NUM 1024
#define MAX_PKG_LEN 1024
#define MAX_BUFFER_SIZE 1024

#define MASTER_IP "192.168.11.114"
#define MASTER_SYNC_TO_SYNC_PORT    8761

#define SLAVE_IP "192.168.11.114"
#define SLAVE_SYNC_TO_SYNC_PORT     8762

#define LOGIN_TIMER_VALUE           (1000*8+1)
#define KEEPALIVE_TIMER_VALUE       (1000*60*3+1)
#define CHECKALIVE_TIMER_VALUE      (1000*60*10+1)
#define NEWCFG_WAITED_TIMER_VALUE   (1000*60*5+1)
#define LIST_TRAVERSE_TIMER_VALUE   (1000*60*5+1)

#endif //_MACRO_H_
