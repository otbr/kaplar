#include "../thread.h"

#include "../log.h"

#include <stdlib.h>
#include <pthread.h>
#include <time.h>


// Thread
// ====================
#define THREAD_COMPLETED 0x01
struct thread{
	pthread_t handle;
	long flags;
	void (*fp)(void*);
	void *arg;
};

static void *wrapper(void *arg)
{
	struct thread *thr = arg;
	if(thr == NULL)
		return NULL;

	thr->fp(thr->arg);
	thr->flags |= THREAD_COMPLETED;
	return thr;
}

int thread_create(struct thread **thr, void (*fp)(void *), void *arg)
{
	int err;

	(*thr) = malloc(sizeof(struct thread));
	(*thr)->fp = fp;
	(*thr)->arg = arg;
	// thread is joinable by default so there is no need to set attributes
	err = pthread_create(&(*thr)->handle, NULL, wrapper, (*thr));
	if(err != 0){
		free(*thr);
		LOG_ERROR("thread_create: failed to create thread (error = %d)", err);
		return -1;
	}
	return 0;
}

int thread_release(struct thread *thr)
{
	if((thr->flags & THREAD_COMPLETED) == 0){
		LOG_ERROR("thread_release: thread is still running");
		return -1;
	}

	// pthreads doesn't need cleaning it seems
	free(thr);
	return 0;
}

int thread_join(struct thread *thr)
{
	int err;

	// thread already completed
	if((thr->flags & THREAD_COMPLETED) != 0)
		return 0;

	err = pthread_join(thr->handle, NULL);
	if(err != 0){
		LOG_ERROR("thread_join: failed to join thread (error = %d)", err);
		return -1;
	}

	return 0;
}

// Mutex
// ====================

struct mutex{
	pthread_mutex_t handle;
};

void mutex_create(struct mutex **mtx)
{
	pthread_mutexattr_t attr;

	(*mtx) = malloc(sizeof(struct mutex));
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&(*mtx)->handle, &attr);
}

void mutex_destroy(struct mutex *mtx)
{
	pthread_mutex_destroy(&mtx->handle);
	free(mtx);
}

void mutex_lock(struct mutex *mtx)
{
	pthread_mutex_lock(&mtx->handle);
}

void mutex_unlock(struct mutex *mtx)
{
	pthread_mutex_unlock(&mtx->handle);
}

// Condition Variable
// ====================
struct condvar{
	pthread_cond_t handle;
};

void condvar_create(struct condvar **cond)
{
	(*cond) = malloc(sizeof(struct condvar));
	pthread_cond_init(&(*cond)->handle, NULL);
}

void condvar_destroy(struct condvar *cond)
{
	pthread_cond_destroy(&cond->handle);
	free(cond);
}

void condvar_wait(struct condvar *cond, struct mutex *mtx)
{
	pthread_cond_wait(&cond->handle, &mtx->handle);
}

void condvar_timedwait(struct condvar *cond, struct mutex *mtx, long msec)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += (msec / 1000);
	ts.tv_nsec += (msec % 1000) * 1000000;
	pthread_cond_timedwait(&cond->handle, &mtx->handle, &ts);
}

void condvar_signal(struct condvar *cond)
{
	pthread_cond_signal(&cond->handle);
}

void condvar_broadcast(struct condvar *cond)
{
	pthread_cond_broadcast(&cond->handle);
}
