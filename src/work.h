#ifndef WORK_H_
#define WORK_H_

struct work;

void work_init(void);
void work_shutdown(void);

void work_dispatch(void (*fp)(void *), void *arg);

#endif //WORK_H_
