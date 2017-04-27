#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

struct sch_entry;

void			scheduler_init();
void			scheduler_shutdown();
struct sch_entry	*scheduler_add(long delay, void (*fp)(void *), void *arg);
void			scheduler_remove(struct sch_entry *entry);
void			scheduler_reschedule(long delay, struct sch_entry *entry);
void			scheduler_pop(struct sch_entry *entry);

#endif //__SCHEDULER_H__