#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define PROTOCOL_VERSION 100

#define START_FLAG (0xaa55)

enum
{
    CMD_LOGIN = 1,
    CMD_NEW_CFG = 2,
    CMD_KEEP_ALIVE = 3
};

#pragma pack(push, 1)
/*
 * message header definition
 */
typedef struct
{
    short signature;
    char cmd;
    char seq;
    short length;
}__attribute__((__packed__)) MSG_HEADER;

#define MSG_HEADER_LEN sizeof(MSG_HEADER)

/*
 * message login structure
 */
typedef struct
{
    MSG_HEADER header;
    char specifyNum;
}__attribute__((__packed__)) MSG_LOGIN_REQ;

typedef struct
{
    MSG_HEADER header;
    char specifyNum;
}__attribute__((__packed__)) MSG_LOGIN_RSP;

/*
 * message new config structure
 */
typedef struct
{
    MSG_HEADER header;
    short newCfgNum;
    short checksum;
    char data[];
}__attribute__((__packed__)) MSG_NEW_CFG_REQ;

enum
{
    NEW_CFG_RESULT_SUCCEED = 0,
    NEW_CFG_RESULT_FAILED = 1
};

typedef struct
{
    MSG_HEADER header;
    short newCfgNum;
    char result;
}__attribute__((__packed__)) MSG_NEW_CFG_RSP;

/*
 * message keep alive structure
 */
typedef struct
{
    MSG_HEADER header;
    char specifyNum;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_REQ;

typedef struct
{
    MSG_HEADER header;
    char specifyNum;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_RSP;

#pragma pack(pop)

#endif //_PROTOCOL_H_