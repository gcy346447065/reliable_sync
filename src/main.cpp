#include <unistd.h> //for read STDIN_FILENO
#include <stdio.h> //for sscanf sprintf
#include <stdlib.h> //for malloc
#include <string.h> //for strcmp memset strstr
#include <fcntl.h> //for open
#include <sys/epoll.h> //for epoll
#include <errno.h> //for errno
#include <sys/socket.h> //for recv
#include "macro.h"
#include "log.h"
#include "master.h"
#include "slave.h"


/*
 * 主备服务的主线程，主要负责监听创建具体服务的请求
 */
INT main(INT argc, CHAR *argv[])
{
    //现在用的是syslog输出到/var/log/local1.log文件中，如有其他打印log方式可代之
    log_init("");
    log_info("Task Beginning.");

    if(argc < 2)
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }

    DWORD dwRet = 0;
    INT iMasterAddr = 0;
    INT iSlaveAddr = 0;
    if(strcmp(argv[1], "master") == SUCCESS)
    {
        log_init("MASTER");
        log_info("MASTER Task.");

        if(sscanf(argv[2], "%d", &iMasterAddr) != 1)
        {
            log_error("master addr error!");
            log_free();
            return FAILE;
        }

        //模拟master开机初始化
        master_InitAndLoop((BYTE)iMasterAddr);

        //模拟master关机
        master_Free();
    }
    else if(strcmp(argv[1], "slave") == SUCCESS)
    {
        log_init("SLAVE");
        log_info("SLAVE Task.");

        if(sscanf(argv[2], "%d", &iMasterAddr) != 1)
        {
            log_error("master addr error!");
            log_free();
            return FAILE;
        }
        if(sscanf(argv[3], "%d", &iSlaveAddr) != 1)
        {
            log_error("slave addr error!");
            log_free();
            return FAILE;
        }

        //模拟slave开机初始化
        slave_InitAndLoop((BYTE)iMasterAddr, (BYTE)iSlaveAddr);

        //模拟slave关机
        slave_Free();
    }
    else
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }

    /* free all */
    log_info("Task Ending.");
    log_free();
    return FAILE;
}


