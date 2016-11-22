#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define PROTOCOL_VERSION 100

#define START_FLAG (0xaa55)

enum
{
    CMD_LOGIN = 1,
    CMD_MAIN_TO_SYNC = 2,
    CMD_SYNC_TO_MAIN = 3,
    CMD_SYNC_TO_SYNC = 4
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
 * message main to sync structure
 */
typedef struct
{
    MSG_HEADER header;
    char checkCRC;
    char data[];
}__attribute__((__packed__)) MSG_MAIN_TO_SYNC_REQ;

typedef struct
{
    MSG_HEADER header;
    char result;
}__attribute__((__packed__)) MSG_MAIN_TO_SYNC_RSP;

#pragma pack(pop)

#endif //_PROTOCOL_H_