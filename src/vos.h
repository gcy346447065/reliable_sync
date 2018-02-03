#ifndef _VOS_H_
#define _VOS_H_

#include "macro.h"

#include <map>
//using std::map;
using namespace std;

typedef DWORD (*TASK_FUNC)(void *pArg);

typedef struct
{
    DWORD dwTaskEventFd;
    TASK_FUNC pTaskFunc;
    void *pArg;
}VOS_TASK_S;

class vos
{
public:
    BYTE byLogNum;

    vos(BYTE byNum)
    {
        byLogNum = byNum;
    }

    DWORD vos_Init();
    void  vos_free();
    DWORD vos_RegTask(const CHAR *pcTaskName, DWORD dwTaskEventFd, TASK_FUNC pTaskFunc, void *pArg);//dwTaskEventFd在创建相应事件句柄时确定
    DWORD vos_DisregTask(const CHAR *pcTaskName);
    DWORD vos_EpollWait();

private:
    DWORD dwEpollFd;
    map<const CHAR *, VOS_TASK_S> mapTask; //pcTaskName -> VOS_TASK_S

    DWORD vos_addEvent(DWORD dwEventFd, bool bETorLT);
    DWORD vos_deleteEvent(DWORD dwEventFd);

};

#endif //_VOS_H_

