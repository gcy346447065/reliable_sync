#ifndef _EVENT_H_
#define _EVENT_H_

#include <stdint.h> //for unit64_t

/* event flags for eventfd */
#define MASTER_EVENT_NULL               0x0000
#define MASTER_EVENT_KEYIN_INSTANT      0x0001
#define MASTER_EVENT_KEYIN_WAITED       0x0002
#define MASTER_EVENT_SLAVE_RESTART      0x0004
#define MASTER_EVENT_CHECKALIVE_TIMER   0x0008
#define MASTER_EVENT_NEWCFG_INSTANT     0x0010
#define MASTER_EVENT_NEWCFG_WAITED      0x0020

int event_init(unsigned int iInitVal);

int event_getEventFlags(int iEventFd, uint64_t *puiEventRead);
int event_setEventFlags(int iEventFd, uint64_t uiEventFlag);
int event_resetEventFlags(int iEventFd, uint64_t uiEventFlag);

#endif //_EVENT_H_