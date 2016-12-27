#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#define PROTOCOL_VERSION 100

#define START_FLAG (0xaa55)

enum
{
    CMD_LOGIN = 1,
    CMD_NEWCFG_INSTANT = 2,
    CMD_NEWCFG_WAITED = 3,
    CMD_KEEP_ALIVE = 4
};

#pragma pack(push, 1)
/*
 * message header definition
 */
typedef struct
{
    short sSignature;
    char cCmd;
    char cSeq;
    short sLength;
}__attribute__((__packed__)) MSG_HEADER;

#define MSG_HEADER_LEN sizeof(MSG_HEADER)

/*
 * message login structure
 */
typedef struct
{
    MSG_HEADER msgHeader;
    char cSynFlag;
    char cAckFlag;
    char cSpecifyID;
}__attribute__((__packed__)) MSG_LOGIN_REQ;

typedef struct
{
    MSG_HEADER msgHeader;
    char cSynAckFlag;
    char cSpecifyID;
}__attribute__((__packed__)) MSG_LOGIN_RSP;

/*
 * message new config instant structure
 */
typedef struct
{
    MSG_HEADER msgHeader;
    int iNewCfgID;
    short sChecksum;
    char acData[];
}__attribute__((__packed__)) MSG_NEWCFG_INSTANT_REQ;

enum
{
    NEWCFG_RESULT_SUCCEED = 0,
    NEWCFG_RESULT_CHECKSUM_ERROR = 1,
    NEWCFG_RESULT_STATUS_ERROR = 2
};

typedef struct
{
    MSG_HEADER msgHeader;
    int iNewCfgID;
    char cResult;
}__attribute__((__packed__)) MSG_NEWCFG_INSTANT_RSP;

/*
 * message new config waited structure
 */
typedef struct
{
    int iNewCfgID;
    short sChecksum;
    int iDataLen;
    char acData[];
}__attribute__((__packed__)) DATA_NEWCFG;

typedef struct
{
    MSG_HEADER msgHeader;
    short sAllChecksum;
    DATA_NEWCFG dataNewcfg[];
}__attribute__((__packed__)) MSG_NEWCFG_WAITED_REQ;

typedef struct
{
    MSG_HEADER msgHeader;
    char cAllResult;
    int aiAgainNewcfgID[];
}__attribute__((__packed__)) MSG_NEWCFG_WAITED_RSP;

/*
 * message keep alive structure
 */
typedef struct
{
    MSG_HEADER msgHeader;
    char cSpecifyNum;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_REQ;

typedef struct
{
    MSG_HEADER msgHeader;
    char cSpecifyNum;
}__attribute__((__packed__)) MSG_KEEP_ALIVE_RSP;

#pragma pack(pop)

#endif //_PROTOCOL_H_