#ifndef __WORK_H__
#define __WORK_H__

struct work;

void work_init();
void work_shutdown();

void work_dispatch(void (*fp)(void *), void *arg);

#endif //__WORK_H__