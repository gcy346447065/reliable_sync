#include "macro.h"
#include "log.h"


INT main(INT argc, CHAR *argv[])
{
    log_init("TEST");
    log_info("Task Beginning.");

    if(argc != 2)
    {
        log_error("main arg error!");
        log_free();
        return FAILE;
    }

    if(sscanf(argv[1], "%d", &iMasterAddr) != 1)
    {
        log_error("master addr error!");
        log_free();
        return FAILE;
    }



    g_pTestVos->VOS_EpollWait(); //while(1)!!!

    return SUCCESS;
}