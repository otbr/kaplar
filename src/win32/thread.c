#include "../log.h"

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <process.h>

// Thread
// ====================
struct thread{
	HANDLE handle;
	void (*fp)(void *);
	void *arg;
};

static unsigned int __stdcall wrapper(void *arg)
{
	struct thread *thr = arg;
	if(thr == NULL)
		return -1;
	thr->fp(thr->arg);
	return 0;
}

int thread_create(struct thread **thr, void (*fp)(void *), void *arg)
{
	(*thr) = malloc(sizeof(struct thread));
	(*thr)->fp = fp;
	(*thr)->arg = arg;
	(*thr)->handle = (HANDLE)_beginthreadex(NULL, 0, wrapper, (*thr), 0, NULL);
	if((*thr)->handle == NULL){
		free(*thr);
		LOG_ERROR("thread_create: failed to create thread (error = %d)", errno);
		return -1;
	}

	return 0;
}

int thread_release(struct thread *thr)
{
	// check if thread has completed
	if(WaitForSingleObject(thr->handle, 0) != WAIT_OBJECT_0){
		LOG_ERROR("thread_release: thread is still running");
		return -1;
	}

	CloseHandle(thr->handle);
	free(thr);
	return 0;
}

int thread_join(struct thread *thr)
{
	if(WaitForSingleObject(thr->handle, INFINITE) == WAIT_FAILED){
		LOG_ERROR("thread_join: failed to join thread (error = %d)", GetLastError());
		return -1;
	}

	return 0;
}

// Mutex
// ====================
struct mutex{
	CRITICAL_SECTION handle;
};

void mutex_create(struct mutex **mtx)
{
	(*mtx) = malloc(sizeof(struct mutex));
	InitializeCriticalSection(&(*mtx)->handle);
}

void mutex_destroy(struct mutex *mtx)
{
	DeleteCriticalSection(&mtx->handle);
	free(mtx);
}

void mutex_lock(struct mutex *mtx)
{
	EnterCriticalSection(&mtx->handle);
}

void mutex_unlock(struct mutex *mtx)
{
	LeaveCriticalSection(&mtx->handle);
}

// Condition Variable
// ====================
struct condvar{
	CONDITION_VARIABLE handle;
};

void condvar_create(struct condvar **cv)
{
	(*cv) = malloc(sizeof(struct condvar));
	InitializeConditionVariable(&(*cv)->handle);
}

void condvar_destroy(struct condvar *cv)
{
	// it seems there is no DeleteConditionVariable
	free(cv);
}

void condvar_wait(struct condvar *cv, struct mutex *mtx)
{
	SleepConditionVariableCS(&cv->handle, &mtx->handle, INFINITE);
}

void condvar_timedwait(struct condvar *cv, struct mutex *mtx, long msec)
{
	SleepConditionVariableCS(&cv->handle, &mtx->handle, (DWORD)msec);
}

void condvar_signal(struct condvar *cv)
{
	WakeConditionVariable(&cv->handle);
}

void condvar_broadcast(struct condvar *cv)
{
	WakeAllConditionVariable(&cv->handle);
}
