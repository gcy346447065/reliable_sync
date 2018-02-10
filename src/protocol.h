#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "macro.h"

//将主机业务线程向主备线程下发的消息，与主备机主备线程间的消息此关键字设置得不一样
#define START_SIG_1 (0xAA55) // task to master, task to slave
#define START_SIG_2 (0xBB66) // slave to master, master to slave
#define START_SIG_3 (0xCC77) //

/*
 * Version Changelog
 * 1.0.0: the base
 */

//每个位不多于4位即不大于15
#define VERSION_MAJOR   1
#define VERSION_MINOR   0
#define VERSION_MICRO   0

#define VERSION_CAL(a, b, c)    (a << 8 | b << 4 | c)
#define VERSION_DOT(a, b, c)    a##.##b##.##c
#define VERSION(a, b, c)        VERSION_DOT(a, b, c)

#define STRINGIFY(s)            TOSTRING(s)
#define TOSTRING(s) #s
#define VERSION_STR             STRINGIFY(VERSION(VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO))
#define VERSION_INT             VERSION_CAL(VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO)

enum
{
    CMD_LOGIN = 1,
    CMD_KEEP_ALIVE = 2,
    CMD_DATA_BATCH = 3,
    CMD_DATA_INSTANT = 4,
    CMD_DATA_WAITED = 5,
    CMD_GET_DATA_COUNT = 6,
};

#pragma pack(push, 1)
/* 
 *class defined 
 */
 
// slave状态
typedef struct
{
    BYTE byBatchFlag;
    BYTE bySendtimes;
    
    DWORD dwDataNums;
    vector<DWORD> vecDataIDs;   //用于记录slave未收到的batch pkg
    
    DWORD dwDataStart;
    DWORD dwDataEnd;
    BYTE* pbyBitmap;  //slave在batch的时候统计收到的batch pkg
    
}SLAVE_BATCH_STATE_S;

typedef struct
{
    BYTE byInstantFlag;
    BYTE bySendtimes;
    
}SLAVE_INSTANT_STATE_S;

typedef struct
{
    BYTE byWaitedFlag;
    BYTE bySendtimes;
    
}SLAVE_WAITED_STATE_S;

typedef struct
{
    WORD wSlvAddr;
    SLAVE_BATCH_STATE_S stBatch;
    SLAVE_INSTANT_STATE_S stInstant;
    SLAVE_WAITED_STATE_S stWaited;
    
}SLAVE_S;

/*
 * message header definition
 */
typedef struct
{
    WORD wSig;      //消息包起始标志signature
    WORD wVer;      //消息包版本号version，比如1.0.0即1<<8+0<<4+0=256
    WORD wSrcAddr;  //消息包起点地址source address，本应该只需要BYTE即可，这里作预留考虑，下同
    WORD wDstAddr;  //消息包终点地址destination address
    DWORD dwSeq;    //消息包序列号sequence，60M/1K=60K接近于WORD，故而将其设置更大
    WORD wCmd;      //消息包命令字command
    WORD wLen;      //消息包长度length
}__attribute__((__packed__)) MSG_HDR_S;

#define MSG_HDR_LEN sizeof(MSG_HDR_S)

/*
 * message login structure
 */
typedef struct
{
    MSG_HDR_S stMsgHdr;
}__attribute__((__packed__)) MSG_LOGIN_REQ_S;

enum
{
    LOGIN_RESULT_SUCCEED = 0,
    LOGIN_RESULT_ERROR = 1,
    LOGIN_RESULT_REGED = 2
};

typedef struct
{
    MSG_HDR_S stMsgHdr;
    BYTE byLoginResult;
}__attribute__((__packed__)) MSG_LOGIN_RSP_S;

/*
 * message keep alive structure
 */
typedef struct
{
    MSG_HDR_S stMsgHdr;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_REQ_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_RSP_S;

/*
 * message data structure
 */
typedef struct
{
    DWORD dwDataID;     //数据ID，60M/1K=60K接近于WORD，故而将其设置更大
    WORD wDataLen;      //数据长度
    WORD wDataChecksum; //数据校验和，在主机主备线程中才会填入，在备机中用于检验
    BYTE abyData[];     //数据消息包
}__attribute__((__packed__)) DATA_PKG_S;

typedef struct
{
    DWORD dwDataStart;   //起始ID，Batch数据才会用到
    DWORD dwDataEnd;     //终止ID，Batch数据才会用到
    DATA_PKG_S stData;
}__attribute__((__packed__)) DATA_BATCH_PKG_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_BATCH_PKG_S stData;
}__attribute__((__packed__)) MSG_DATA_BATCH_REQ_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_PKG_S stData;
}__attribute__((__packed__)) MSG_DATA_INSTANT_REQ_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_PKG_S stData;
}__attribute__((__packed__)) MSG_DATA_WAITED_REQ_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DWORD dwDataCount;
    DATA_PKG_S astDatas[];
}__attribute__((__packed__)) MSG_DATA_WAITED_PKGS_REQ_S;

enum
{
    DATA_RESULT_SUCCEED = 0,
    DATA_RESULT_CHECKSUM_ERROR = 1,
    DATA_RESULT_STATUS_ERROR = 2
};

typedef struct
{
    DWORD dwDataID;      //数据ID，60M/1K=60K接近于WORD，故而将其设置更大
    BYTE byResult;
}__attribute__((__packed__)) DATA_RESULT_S;


typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_RESULT_S stDataResult;
}__attribute__((__packed__)) MSG_DATA_BATCH_RSP_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_RESULT_S stDataResult;
}__attribute__((__packed__)) MSG_DATA_INSTANT_RSP_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_RESULT_S stDataResult;
}__attribute__((__packed__)) MSG_DATA_WAITED_RSP_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DWORD dwDataCount;
    DATA_RESULT_S astDataResults[];
}__attribute__((__packed__)) MSG_DATA_WAITED_PKGS_RSP_S;

// 该报文是slave向master发的batch回复报文
typedef struct
{
    DWORD dwDataStart;
    DWORD dwDataEnd;
    DWORD dwNeedPkgNums;
    DWORD dwDataIDs[];      //数据ID列表，为slave未收到的batch包集合
}__attribute__((__packed__)) DATA_SLAVE_RECV_BATCH_RESULT_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DATA_SLAVE_RECV_BATCH_RESULT_S stSlvRecvResult;
}__attribute__((__packed__)) MSG_DATA_SLAVE_BATCH_RSP_S;


#define MAX_SEND_TIMES 3

typedef struct
{
    DATA_BATCH_PKG_S stBatchNet;//网络序的数据
}__attribute__((__packed__)) NODE_DATA_BATCH_S;

typedef struct
{
    DATA_PKG_S stInstantNet;//网络序的数据
}__attribute__((__packed__)) NODE_DATA_INSTANT_S;

typedef struct
{
    DATA_PKG_S stWaitedNet;//网络序的数据
}__attribute__((__packed__)) NODE_DATA_WAITED_S;


/*
 * message get data count structure
 */
typedef struct
{
    MSG_HDR_S stMsgHdr;
}__attribute__((__packed__)) MSG_GET_DATA_COUNT_REQ_S;

typedef struct
{
    MSG_HDR_S stMsgHdr;
    DWORD dwBatchCount;
    DWORD dwInstantCount;
    DWORD dwWaitedCount;
}__attribute__((__packed__)) MSG_GET_DATA_COUNT_RSP_S;


#pragma pack(pop)

#endif //_PROTOCOL_H_

