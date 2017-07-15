#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include "macro.h"

#define PROTOCOL_VERSION 100

#define START_FLAG (0xAA55)
//#define START_FLAG_2 (0xBB66)
//#define START_FLAG_3 (0xCC77)

enum
{
    CMD_LOGIN = 1,
    CMD_KEEP_ALIVE = 2,
    CMD_NEWCFG_BATCH = 3,
    CMD_NEWCFG_INSTANT = 4,
    CMD_NEWCFG_WAITED = 5
};

#pragma pack(push, 1)
/*
 * message header definition
 */
typedef struct
{
    WORD wSig;  //消息包起始标志signature
    BYTE byOrigAddr; //消息包起点地址origin address
    BYTE byDestAddr; //消息包终点地址destination address
    BYTE bySeq; //消息包序列号sequence
    BYTE byCmd; //消息包命令字command
    WORD wLen;  //消息包长度length
}__attribute__((__packed__)) MSG_HEADER_S;

#define MSG_HEADER_LEN sizeof(MSG_HEADER_S)

enum
{
    LOGIN_END_FLAG_DISABLED = 0,
    LOGIN_END_FLAG_ENABLED = 1
};

/*
 * message login structure
 */
typedef struct
{
    MSG_HEADER_S stMsgHeader;
    BYTE byEndFlag;
}__attribute__((__packed__)) MSG_LOGIN_REQ_S;

enum
{
    LOGIN_RESULT_SUCCEED = 0,
    LOGIN_RESULT_ERROR = 1
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
    BYTE byKeepAliveTimes;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_REQ_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    BYTE byKeepAliveTimes;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_RSP_S;

/*
 * message new config instant structure
 */
typedef struct
{
    MSG_HEADER_S stMsgHeader;
    unsigned int uiNewcfgID;
    short sChecksum;
    char acData[];
}__attribute__((__packed__)) MSG_NEWCFG_INSTANT_REQ_S;

enum
{
    NEWCFG_RESULT_SUCCEED = 0,
    NEWCFG_RESULT_CHECKSUM_ERROR = 1,
    NEWCFG_RESULT_STATUS_ERROR = 2
};

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    unsigned int uiNewcfgID;
    char cResult;
}__attribute__((__packed__)) MSG_NEWCFG_INSTANT_RSP_S;

/*
 * message new config waited structure
 */
typedef struct
{
    unsigned int uiNewcfgID;
    short sChecksum;
    int iDataLen;
    char acData[];
}__attribute__((__packed__)) DATA_NEWCFG;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    short sChecksum;
    unsigned int uiWaitedSum;
    DATA_NEWCFG dataNewcfg[];
}__attribute__((__packed__)) MSG_NEWCFG_WAITED_REQ_S;

typedef struct
{
    MSG_HEADER_S stMsgHeader;
    unsigned int auiNewcfgID[];
}__attribute__((__packed__)) MSG_NEWCFG_WAITED_RSP_S;

#pragma pack(pop)


#endif //_PROTOCOL_H_
