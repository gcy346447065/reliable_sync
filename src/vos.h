#ifndef _VOS_H_
#define _VOS_H_

#include "macro.h"

#define VOS_TASK_NULL                   0x00000000
#define VOS_TASK_MASTER_MAILBOX         0x00000001
#define VOS_TASK_SLAVE_MAILBOX          0x00000002
#define VOS_TASK_SLAVE_REGISTER_TIMER   0x00000004
#define VOS_TASK_MASTER_STDIN           0x00000008
#define VOS_TASK_SLAVE_STDIN            0x00000010
#define VOS_TASK_MASTER_KEEPAILVE_TIMER 0x00000020

#define VOS_TASK_TEST_STDIN             0x00000040

#define TFP_READ_NOTIFY_EVENT 0
#define OS_EV_ANY 0
#define VOS_WAIT_FOREVER 0


typedef DWORD (*TASK_FUNC)(void *pObj);

typedef struct
{
    DWORD dwTaskMacro;
    DWORD dwTaskEventFd;
    TASK_FUNC func;
}VOS_TASK_S;

class vos
{
public:
    DWORD g_dwMapCount;
    DWORD g_dwEpollFd;
    DWORD g_dwTaskMacros;
    VOS_TASK_S g_vosTaskMap[32];//macro、event、func对应关系表
    
    //本来是第一个参数是const CHAR *pszTaskName，为了方便比对，故而修改成宏定义
    DWORD VOS_RegTaskEventFd(DWORD dwTaskMacro, DWORD dwTaskEventFd);
    DWORD VOS_RegTaskFunc(DWORD dwTaskMacro, TASK_FUNC taskFunc, void *pArg);

    DWORD VOS_ReceiveEvent(DWORD dwTargetEvents, DWORD dwEvAny, DWORD dwWaitForever, DWORD *pdwEvent);

    DWORD VOS_Init();
    DWORD VOS_EpollWait();

};

#endif //_VOS_H_
