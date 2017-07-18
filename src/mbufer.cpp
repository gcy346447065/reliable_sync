#include <sys/socket.h> //for socket
#include <netinet/in.h> //for sockaddr_in htons
#include <arpa/inet.h> //for inet_addr
#include <sys/ioctl.h> //for ioctl
#include <string.h> //for memset
#include <errno.h> //for errno
#include <unistd.h> //for close
#include <stdlib.h> //for malloc
#include "mbufer.h"
#include "log.h"


//实际只使用了ppDmmMailbox、stMailboxAddr、dwTaskMacro三个参数
DWORD dmm::create_mailbox(mbufer **ppMbufer, BYTE byMsgAddr)
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
    switch(byMsgAddr)//从创建邮箱的地址映射出实际使用的ip:port
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
            log_error("byMsgAddr error(%d)!", byMsgAddr);
            return FAILE;
    }
    //绑定本端地址
    if(bind(iSockFd, (struct sockaddr *)&stLclAddr, sizeof(stLclAddr)) < 0)
    {
        log_error("socket bind error(%d)!", errno);
        return FAILE;
    }
    
    (*ppMbufer)->g_dwSocketFd = iSockFd;//将socket句柄记录在邮箱mbufer中

    return SUCCESS;
}

DWORD dmm::delete_mailbox(mbufer *pMbufer)
{
    close(pMbufer->g_dwSocketFd);
    return SUCCESS;
}


DWORD mbufer::alloc_msg(void **ppSendBuf, WORD wMsgLen)
{
    *ppSendBuf = malloc(wMsgLen);
    if(ppSendBuf == NULL)
    {
        log_error("malloc error!");
        return FAILE;
    }

    return SUCCESS;
}

DWORD mbufer::free_msg(void *pSendBuf)
{
    free(pSendBuf);

    return SUCCESS;
}

DWORD mbufer::set_cmd_head_flag(void *pSendBuf, DWORD dwSendFlag)
{
    DWORD dwRet = 0;

    return dwRet;
}

DWORD mbufer::add_to_packet(void *pSendBuf, CMD_S *pstCmdHeader, WORD *pwOffset)
{
    memcpy(pSendBuf, pstCmdHeader->pbyPara, pstCmdHeader->wParaLen);
    *pwOffset = pstCmdHeader->wParaLen;

    return SUCCESS;
}

DWORD mbufer::send_message(BYTE byDstMsgAddr, MSG_INFO_S stMsgInfo, WORD wOffset)
{
    INT iRet = 0;
    //log_debug("byDstMsgAddr(%d).", byDstMsgAddr);
    struct sockaddr_in stDstAddr;
    memset(&stDstAddr, 0, sizeof(stDstAddr));
    stDstAddr.sin_family = AF_INET;
    switch(byDstMsgAddr)//从创建邮箱的地址映射出实际使用的ip:port
    {
        case ADDR_1:
            stDstAddr.sin_addr.s_addr = inet_addr(IP_1);
            stDstAddr.sin_port = htons(PORT_1);
            break;

        case ADDR_2:
            stDstAddr.sin_addr.s_addr = inet_addr(IP_2);
            stDstAddr.sin_port = htons(PORT_2);
            break;

        case ADDR_3:
            stDstAddr.sin_addr.s_addr = inet_addr(IP_3);
            stDstAddr.sin_port = htons(PORT_3);
            break;

        case ADDR_4:
            stDstAddr.sin_addr.s_addr = inet_addr(IP_4);
            stDstAddr.sin_port = htons(PORT_4);
            break;

        case ADDR_5:
            stDstAddr.sin_addr.s_addr = inet_addr(IP_5);
            stDstAddr.sin_port = htons(PORT_5);
            break;

        default:
            log_error("byDstMsgAddr error(%d)!", byDstMsgAddr);
            return FAILE;
    }

    //log_debug("g_dwSocketFd(%d).", g_dwSocketFd);
    BYTE *pbySendBuf = (BYTE *)(stMsgInfo.dwMsgBuf);
    if((iRet = sendto(g_dwSocketFd, pbySendBuf, wOffset, 0, (struct sockaddr *)&stDstAddr, sizeof(stDstAddr))) < 0)
    {
        log_error("send_message error(%d)!", iRet);
    }
    
    /*if((iRet = sendto(g_dwSocketFd, "test", 4, 0, (struct sockaddr *)&stDstAddr, sizeof(stDstAddr))) < 0)
    {
        log_error("send_message error(%d)!", iRet);
    }*/

    return SUCCESS;
}

DWORD mbufer::receive_message(BYTE *pbyRecvBuf, WORD *pwBufLen, DWORD dwWaitTime)
{
    INT iBufferSize = 0;
    if((iBufferSize = recv(g_dwSocketFd, pbyRecvBuf, MAX_BUFFER_SIZE, 0)) > 0)
    {
        //log_hex(pRecvBuf, iBufferSize);
        *pwBufLen = (WORD)iBufferSize;
        return SUCCESS;
    }
    else if(iBufferSize < 0)
    {
        return FAILE;
    }

    return SUCCESS;
}

DWORD mbufer::get_from_packet(void *pSendBuf, CMD_S *pstCmdHeader, WORD *pwOffset)
{
    return SUCCESS;
}

DWORD mbufer::get_cmd_head_flag(void *pSendBuf, DWORD dwSendFlag)
{
    return SUCCESS;
}

DWORD mbufer::get_msg_sender_addr(void *pMsgBuf, MSG_ADDR_S &rMsgSenderAddr)
{
    return SUCCESS;
}

DWORD mbufer::get_msg_recver_addr(void *pMsgBuf, MSG_ADDR_S &rMsgRecverAddr)
{
    return SUCCESS;
}

DWORD mbufer::get_msg_data_length(void *pMsgBuf, DWORD &rdwMsgLen)
{
    return SUCCESS;
}

