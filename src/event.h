#ifndef _EVENT_H_
#define _EVENT_H_

#include <stdint.h> //for unit64_t

/* g_iMainEventFd 对应的事件 */
#define SLAVE_MAIN_EVENT_NULL                   0x0000
#define SLAVE_MAIN_EVENT_NEWCFG_INSTANT         0x0001
#define SLAVE_MAIN_EVENT_NEWCFG_WAITED          0x0002
#define SLAVE_MAIN_EVENT_MASTER_RESTART         0x0004
#define SLAVE_MAIN_EVENT_CHECKALIVE_TIMER       0x0008

/* g_iSyncEventFd 对应的事件 */
#define SLAVE_SYNC_EVENT_NULL                   0x0000

int event_init(unsigned int iInitVal);

int event_getEventFlags(int iEventFd, uint64_t *puiEventRead);
int event_setEventFlags(int iEventFd, uint64_t uiEventFlag);
int event_resetEventFlags(int iEventFd, uint64_t uiEventFlag);

#endif //_EVENT_H_