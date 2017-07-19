#ifndef _MACRO_H_
#define _MACRO_H_

typedef unsigned long long  QWORD;
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef FLOAT               *PFLOAT;

typedef void                VOID;
typedef int                 INT;
typedef unsigned int        UINT;
typedef unsigned int        *PUINT;
typedef char                CHAR;
typedef char                *PCHAR;

#define SUCCESS             0
#define FAILE               1
#define SLV_HAS_REGED       2

//设置命令包的命令包头信息，被SetCmdHeadFlag和GetCmdHeadFlag
#define CRM_HEADINFO_REQCMD       1        //有响应的请求命令
#define DMM_NO_WAIT               0
#define DMM_SUCCESS               0
#define ERR_CRM_SUCCESS           0
#define VOS_OK                    0

enum 
{
    STATUS_INIT = 0,
    STATUS_LOGIN = 1,
    STATUS_NEWCFG = 2
};

enum SEND_METHOD_TO_LIST
{
    SEND_NEWCFG_WAITED,
    SEND_NEWCFG_INSTANT
};

#define MAX_PKG_LEN 900
//#define MAX_PKG_LEN 1024

#define MAX_STDIN_FILE_LEN 128
#define MAX_EPOLL_NUM 1024
#define MAX_BUFFER_SIZE 1024

#define ADDR_1      1
#define IP_1        "192.168.11.114"
#define PORT_1      8761

#define ADDR_2      2
#define IP_2        "192.168.11.114"
#define PORT_2      8762

#define ADDR_3      3
#define IP_3        "192.168.11.114"
#define PORT_3      8763

#define ADDR_4      4
#define IP_4        "192.168.11.114"
#define PORT_4      8764

#define ADDR_5      5
#define IP_5        "192.168.11.114"
#define PORT_5      8765

#define ADDR_6      6
#define IP_6        "192.168.11.114"
#define PORT_6      8766

#define ADDR_7      7
#define IP_7        "192.168.11.114"
#define PORT_7      8767

#define REGISTER_TIMER_VALUE        (1000*30+1) //30s
#define KEEPALIVE_TIMER_VALUE       (1000*60*1+1) //3min-1min
#define CHECKALIVE_TIMER_VALUE      (1000*60*10+1)
#define NEWCFG_INSTANT_TIMER_VALUE  (1000*60*5+1)
#define NEWCFG_WAITED_TIMER_VALUE   (1000*60*5+1)

#endif //_MACRO_H_
