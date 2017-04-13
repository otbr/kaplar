#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

struct sch_entry;

void scheduler_init();
void scheduler_shutdown();

struct sch_entry *scheduler_add(long delay, void (*fp)(void *), void *arg);
void scheduler_rem();

#endif //__SCHEDULER_H__