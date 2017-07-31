#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "macro.h"

#define PROTOCOL_VERSION 100

#define START_FLAG_1 (0xAA55)
#define START_FLAG_2 (0xBB66)
#define START_FLAG_3 (0xCC77)

enum
{
    CMD_LOGIN = 1,
    CMD_KEEP_ALIVE = 2,
    CMD_DATA_INSTANT = 3,
    CMD_DATA_WAITED = 4
};

#pragma pack(push, 1)
/*
 * message header definition
 */
typedef struct
{
    WORD wSig;  //消息包起始标志signature
    BYTE bySrcAddr; //消息包起点地址source address
    BYTE byDstAddr; //消息包终点地址destination address
    WORD wSeq; //消息包序列号sequence
    WORD wCmd; //消息包命令字command
    WORD wLen;  //消息包长度length
}__attribute__((__packed__)) MSG_HEADER_S;

#define MSG_HEADER_LEN sizeof(MSG_HEADER_S)

/*
 * message login structure
 */
typedef struct
{
    MSG_HEADER_S stMsgHeader;
}__attribute__((__packed__)) MSG_LOGIN_REQ_S;

enum
{
    LOGIN_RESULT_SUCCEED = 0,
    LOGIN_RESULT_ERROR = 1,
    LOGIN_RESULT_REGED = 2
};

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    BYTE byLoginResult;
}__attribute__((__packed__)) MSG_LOGIN_RSP_S;

/*
 * message keep alive structure
 */
typedef struct
{
    MSG_HEADER_S stMsgHeader;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_REQ_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_RSP_S;

/*
 * message data instant structure
 */
enum
{
    DATA_TYPE_BATCH = 1,
    DATA_TYPE_INSTANT = 2,
    DATA_TYPE_WAITED = 3
};

typedef struct
{
    WORD wDataSig;      //数据起始标志signature
    WORD wDataSeq;      //数据序列号sequence
    BYTE byDataType;    //数据类型：BATCH，INSTANT、WAITED
    WORD wDataID;       //数据ID，60M/1K=60K < 65535
    WORD wBatchStart;   //起始ID，Batch数据才会用到
    WORD wBatchEnd;     //终止ID，Batch数据才会用到
    WORD wDataLen;      //数据长度
    WORD wDataChecksum; //数据校验和，在主机中才会填入，在备机中用于检验
    BYTE abyData[];     //数据消息
}__attribute__((__packed__)) MSG_DATA_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    MSG_DATA_S stData;
}__attribute__((__packed__)) MSG_DATA_INSTANT_REQ_S;

enum
{
    DATA_RESULT_SUCCEED = 0,
    DATA_RESULT_CHECKSUM_ERROR = 1,
    DATA_RESULT_STATUS_ERROR = 2
};

typedef struct
{
    WORD wDataSeq;
    BYTE byDataType;
    WORD wDataID;
    BYTE byResult;
}__attribute__((__packed__)) DATA_RESULT_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    DATA_RESULT_S stDataResult;
}__attribute__((__packed__)) MSG_DATA_INSTANT_RSP_S;

/*
 * message data waited structure
 */
typedef struct
{
    MSG_HEADER_S stMsgHeader;
    WORD wDataCount;
    MSG_DATA_S astDatas[];
}__attribute__((__packed__)) MSG_DATA_WAITED_REQ_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    WORD wDataCount;
    DATA_RESULT_S astDataResults[];
}__attribute__((__packed__)) MSG_DATA_WAITED_RSP_S;

#pragma pack(pop)


#endif //_PROTOCOL_H_
