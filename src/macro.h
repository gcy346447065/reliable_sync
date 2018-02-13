#ifndef _MACRO_H_
#define _MACRO_H_

typedef unsigned long long  QWORD;

typedef unsigned int       DWORD; // long在32位机长度为4，在64位机长度为8!!!

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

typedef DWORD (*MSG_PROC)(void *pArg, const void *pMsg);
typedef struct
{
    WORD wCmd;
    MSG_PROC pfn;
} MSG_PROC_MAP;

#define FALSE               0
#define TRUE                1

#define SUCCESS             0
#define FAILE               1
#define SLV_HAS_REGED       2

//设置命令包的命令包头信息，被SetCmdHeadFlag和GetCmdHeadFlag
#define CRM_HEADINFO_REQCMD       1        //有响应的请求命令
#define DMM_NO_WAIT               0
#define DMM_SUCCESS               0
#define ERR_CRM_SUCCESS           0
#define VOS_OK                    0


//包的最小值61853288/65535=943
#define MAX_TASK2MST_PKG_LEN (1400) //规定单包大小不能超过1500
#define MAX_PKG_LEN 1000
//给入的单个instant配置文件最大长度，若超过则需要走batch
#define MAX_FILE_LEN 1500

//us
//#define MAX_TASK2MST_RECV_TIMEOUT (10 * 1000 * 1000)
#define MAX_TASK2MST_RECV_TIMEOUT (10 * 1000 * 1000)

//MAX_RECV_LEN一定要比MAX_PKG_LEN大，考虑到MTU为1500，这里可设计为1536
#define MAX_TASK2MST_RECV_LEN (MAX_TASK2MST_PKG_LEN + 64) 
#define MAX_RECV_LEN 1536 

//slave发送batch回复包的最大包数，及最大重传次数
#define MAX_SLAVE_RES_BATCH_PKGS 300    //(MAX_TASK2MST_PKG_LEN - (sizeof(MSG_DATA_SLAVE_BATCH_RSP_S) - sizeof(MSG_HDR_S))) / sizeof(DWORD) = 347
#define MAX_RETRANS_TIMES 3

#define MAX_STDIN_FILE_LEN 128
#define MAX_EPOLL_NUM 64

//0和11是task的默认地址
#define ADDR_MstTask  0
#define IP_0        "192.168.11.114"
#define PORT_0      8760

#define ADDR_1_114  1
#define IP_1        "192.168.11.114"
#define PORT_1      8761

#define ADDR_2_114  2
#define IP_2        "192.168.11.114"
#define PORT_2      8762

#define ADDR_3_114  3
#define IP_3        "192.168.11.114"
#define PORT_3      8763

#define ADDR_4_114  4
#define IP_4        "192.168.11.114"
#define PORT_4      8764

#define ADDR_5_114  5
#define IP_5        "192.168.11.114"
#define PORT_5      8765

#define ADDR_6_119  6
#define IP_6        "192.168.11.119"
#define PORT_6      8766

#define ADDR_7_119  7
#define IP_7        "192.168.11.119"
#define PORT_7      8767

#define ADDR_8_119  8
#define IP_8        "192.168.11.119"
#define PORT_8      8768

#define ADDR_9_119  9
#define IP_9        "192.168.11.119"
#define PORT_9      8769

#define ADDR_10_119 10
#define IP_10       "192.168.11.119"
#define PORT_10     8770

#define ADDR_SlvTask1 11
#define IP_11       "192.168.11.119"
#define PORT_11     8771

#define ADDR_MIN    ADDR_1_114
#define ADDR_MAX    ADDR_10_119



#define REGISTER_TIMER_VALUE            (1000*30+1) //30s
#define KEEPALIVE_TIMER_VALUE           (1000*60*3+1) //3min-1min
#define CHECKALIVE_TIMER_VALUE          (1000*60*10+1)

#define NEWCFG_BATCH_TIMER_VALUE        (1000*7+1)  //7s  slave's time
#define NEWCFG_MST_BATCH_TIMER_VALUE    (1000*10+1) //10s master's time
#define NEWCFG_BATCH_FAST_TIMER_VALUE   (1000*3+1) //3s

#define NEWCFG_INSTANT_TIMER_VALUE      (1000*60*5+1)
#define NEWCFG_WAITED_TIMER_VALUE       (1000*60*5+1)

#endif //_MACRO_H_

