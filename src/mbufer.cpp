#include <sys/socket.h> //for socket
#include <netinet/in.h> //for sockaddr_in htons
#include <arpa/inet.h> //for inet_addr
#include <sys/ioctl.h> //for ioctl
#include <string.h> //for memset
#include <errno.h> //for errno
#include "mbufer.h"
#include "log.h"

//实际只使用了ppDmmMailbox、stMailboxAddr、dwTaskMacro三个参数
DWORD dmm::create_mailbox(mbufer **ppDmmMailbox, MSG_ADDR stMailboxAddr)
{
    /* 创建UDP的socket句柄 */
    INT iSockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if(iSockFd < 0)
    {
        log_error("Create socket error!");
        return FAILE;
    }

    /* 将socket设置为非阻塞模式 */
    INT iMode= 1; 
    INT iCtlRet = ioctl(iSockFd, FIONBIO, &iMode);
    if(iCtlRet < 0)
    {
        log_error("Set socket no block error!");
        return FAILE;
    }

    struct sockaddr_in stLclAddr;
    memset(&stLclAddr, 0, sizeof(stLclAddr));
    stLclAddr.sin_family = AF_INET;
    switch(stMailboxAddr.byEntryID)//从创建邮箱的地址映射出实际使用的ip:port
    {
        case ADDR_1:
            stLclAddr.sin_addr.s_addr = inet_addr(IP_1);
            stLclAddr.sin_port = htons(PORT_1);
            break;

        case ADDR_2:
            stLclAddr.sin_addr.s_addr = inet_addr(IP_2);
            stLclAddr.sin_port = htons(PORT_2);
            break;

        case ADDR_3:
            stLclAddr.sin_addr.s_addr = inet_addr(IP_3);
            stLclAddr.sin_port = htons(PORT_3);
            break;

        case ADDR_4:
            stLclAddr.sin_addr.s_addr = inet_addr(IP_4);
            stLclAddr.sin_port = htons(PORT_4);
            break;

        case ADDR_5:
            stLclAddr.sin_addr.s_addr = inet_addr(IP_5);
            stLclAddr.sin_port = htons(PORT_5);
            break;

        default:
            log_error("stMailboxAddr.byEntryID error(%d)!", stMailboxAddr.byEntryID);
            return FAILE;
    }
    //绑定本端地址
    if(bind(iSockFd, (struct sockaddr *)&stLclAddr, sizeof(stLclAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return FAILE;
    }
    
    (*ppDmmMailbox)->g_dwMbuferFd = iSockFd;//将socket句柄记录在邮箱mbufer中

    return SUCCESS;
}

DWORD dmm::delete_mailbox(mbufer *pDmmMailbox)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::alloc_msg(void **ppSendBuf, UINT uiMsgLen)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::free_msg(void *pSendBuf)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::set_cmd_head_flag(void *pSendBuf, DWORD dwSendFlag)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::add_to_packet(void *pSendBuf, CMD_S *pstCmdHeader, DWORD *pdwOffset)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::send_message(MSG_ADDR stDstAddr, MSG_INFO stSendMsg, DWORD dwOffset)
{
    DWORD dwRet = 0;

    return dwRet;
}
