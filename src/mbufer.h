#ifndef _MBUFER_H_
#define _MBUFER_H_

#include "macro.h"

struct CMD_S
{
    WORD wCmdIdx;       // 命令字索引
    WORD wCtrlCmd;      // 控制字
    union
    {
        WORD wCmd;      // 命令字
        WORD wEvent;    // 事件
    };
    WORD wParaLen;      // 参数长度
    BYTE *pbyPara;      // 参数指针
};

struct MSG_ADDR
{
    DWORD dwNeID;       // 网元号，不再支持网元yao间通信，所以此字段没有意义了----废弃，不用管
    WORD wSoftwareID;   // 模块号----同EntryID一起决定是CPU内部的某个通信实体，相当于第一层寻址
    WORD wCardID;       // (逻辑)板位号----可以对应IP地址，决定用哪个CPU，比如192.168.0.1对应的cardid为1,192.168.0.2对应的cardid为2
    BYTE byEntryID;     // 入口号----同SoftwareID一起决定是CPU内部的某个通信实体，相当于第二层寻址
};

struct MSG_INFO
{
    DWORD dwMsgType;    // 消息类型
    DWORD dwSendBuf;    // 即将发送消息内存地址
    DWORD dwBoardID;    // 目的邮件单板槽位号----暂时不用
    DWORD dwUsrID;      // 通信通道----暂时不用
};


/*
 * 该类用于发送消息包，本地使用socket udp模拟接口
 */
class mbufer
{

public:
    DWORD g_dwMbuferFd;

    DWORD alloc_msg(void **ppSendBuf, UINT uiMsgLen);
    DWORD free_msg(void *pSendBuf);
    DWORD set_cmd_head_flag(void *pSendBuf, DWORD dwSendFlag);
    DWORD add_to_packet(void *pSendBuf, CMD_S *pstCmdHeader, DWORD *pdwOffset);
    DWORD send_message(MSG_ADDR stDstAddr, MSG_INFO stCmdHeader, DWORD dwOffset);
    
    DWORD receive_message(MSG_INFO stCmdHeader, DWORD dwWaitTime);
    DWORD get_from_packet(void *pSendBuf, CMD_S *pstCmdHeader, DWORD *pdwOffset);
    DWORD get_cmd_head_flag(void *pSendBuf, DWORD dwSendFlag);

    DWORD get_msg_sender_addr(void *pMsgBuf, MSG_ADDR &rMsgSenderAddr);
    DWORD get_msg_recver_addr(void *pMsgBuf, MSG_ADDR &rMsgRecverAddr);
    DWORD get_msg_data_length(void *pMsgBuf, DWORD &rdwMsgLen);
};

class dmm
{

public:
    DWORD create_mailbox(mbufer **ppDmmMailbox, MSG_ADDR stMailboxAddr);
    DWORD delete_mailbox(mbufer *pDmmMailbox);
};

#endif //_MBUFER_H_
