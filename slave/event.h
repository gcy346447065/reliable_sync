#ifndef _EVENT_H_
#define _EVENT_H_

#include <stdint.h> //for unit64_t

/* event flags for eventfd */
#define SLAVE_EVENT_NULL                0x0000
#define SLAVE_EVENT_MASTER_NEWCFG       0x0001
#define SLAVE_EVENT_MASTER_RESTART      0x0002
#define SLAVE_EVENT_CHECKALIVE_TIMER    0x0004
#define SLAVE_EVENT_QUEUE_PUSH          0x0008
#define SLAVE_EVENT_SYNC_LOGIN          0x0010


int event_init(unsigned int iInitVal);

int event_getEventFlags(int iEventFd, uint64_t *puiEventRead);
int event_setEventFlags(int iEventFd, uint64_t uiEventFlag);
int event_resetEventFlags(int iEventFd, uint64_t uiEventFlag);

#endif //_EVENT_H_