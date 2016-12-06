#ifndef _TIMER_H_
#define _TIMER_H_

int timer_create(void);
int timer_start(int iTimerFd, int iMS);
int timer_close(int iTimerFd);

#endif //_TIMER_H_