#ifndef _EVENT_H_
#define _EVENT_H_

#include <stdint.h> //for unit64_t

/* event flags for eventfd */
#define EVENT_FLAG_NONE 0x00
#define EVENT_FLAG_MASTER_NEWCFG    0x01
#define EVENT_FLAG_SLAVE_RESTART    0x02
#define EVENT_FLAG_SYNC_TIMER       0x04
#define EVENT_FLAG_QUEUE_PUSH       0x08

int event_init(unsigned int iInitVal);

int event_getEventFlags(int iEventFd, uint64_t *puiEventRead);
int event_setEventFlags(int iEventFd, uint64_t uiEventFlag);
int event_resetEventFlags(int iEventFd, uint64_t uiEventFlag);

#endif //_EVENT_H_