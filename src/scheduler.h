#ifndef SCHEDULER_H_
#define SCHEDULER_H_

struct sch_entry;

void			scheduler_init(void);
void			scheduler_shutdown(void);
struct sch_entry	*scheduler_add(long delay, void (*fp)(void *), void *arg);
void			scheduler_remove(struct sch_entry *entry);
void			scheduler_reschedule(long delay, struct sch_entry *entry);
void			scheduler_pop(struct sch_entry *entry);

#endif //SCHEDULER_H_
