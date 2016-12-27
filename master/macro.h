#ifndef _MACRO_H_
#define _MACRO_H_


#define MAX_EPOLL_NUM 1024
#define MAX_PKG_LEN 1024
#define MAX_BUFFER_SIZE 1024

#define MASTER_IP "192.168.11.114"
#define MASTER_SYNC_TO_SYNC_PORT    8761

#define SLAVE_IP "192.168.11.114"
#define SLAVE_SYNC_TO_SYNC_PORT     8762

#define LOGIN_TIMER_VALUE           (1000*8)
#define KEEPALIVE_TIMER_VALUE       (1000*60*3)
#define CHECKALIVE_TIMER_VALUE      (1000*60*10)


#endif //_MACRO_H_