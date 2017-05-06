#ifndef THREAD_H_
#define THREAD_H_

// thread, mutex and condvar are opaque structs
// and defined on implementation files

// Thread
// ====================
struct thread;

int	thread_create(struct thread **thr, void (*fp)(void *), void *arg);
int	thread_release(struct thread *thr);
int	thread_join(struct thread *thr);

// Mutex
// ====================
// NOTE: the implementation must support recursive lock/unlock
struct mutex;

void	mutex_create(struct mutex **mtx);
void	mutex_destroy(struct mutex *mtx);
void	mutex_lock(struct mutex *mtx);
void	mutex_unlock(struct mutex *mtx);

// Condition Variable
// ====================
struct condvar;

void	condvar_create(struct condvar **cv);
void	condvar_destroy(struct condvar *cv);
void	condvar_wait(struct condvar *cv, struct mutex *mtx);
void	condvar_timedwait(struct condvar *cv, struct mutex *mtx, long msec);
void	condvar_signal(struct condvar *cv);
void	condvar_broadcast(struct condvar *cv);


#endif //THREAD_H_
