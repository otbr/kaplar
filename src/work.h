#ifndef WORK_H_
#define WORK_H_

struct work{
	void (*fp)(void*);
	void *arg;
};

void work_init(void);
void work_shutdown(void);
void work_dispatch(void (*fp)(void*), void *arg);
void work_dispatch_array(int count, int single, struct work *work);

#endif //WORK_H_
