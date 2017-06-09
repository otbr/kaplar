#include "work.h"

#include "thread.h"
#include "system.h"
#include "log.h"
#include "util.h"

#include <stddef.h>

struct work{
	void (*fp)(void*);
	void *arg;
};

// work ring buffer
#define MAX_WORK 4096
static struct work work_pool[MAX_WORK];
static int readpos;
static int writepos;
static int pending_work;

// thread pool
#define MAX_THREADS 64
static struct thread *thread_pool[MAX_THREADS];
static struct mutex *mutex;
static struct condvar *cond;
static int thread_count;
static int running = 0;


static void worker_thread(void *arg)
{
	struct work wrk;
	while(running != 0){
		// retrieve work
		mutex_lock(mutex);
		if(pending_work <= 0){
			condvar_wait(cond, mutex);
			if(pending_work <= 0){
				mutex_unlock(mutex);
				continue;
			}
		}
		wrk.fp = work_pool[readpos].fp;
		wrk.arg = work_pool[readpos].arg;
		++readpos;
		if(readpos >= MAX_WORK)
			readpos = 0;
		--pending_work;
		mutex_unlock(mutex);

		// execute work
		wrk.fp(wrk.arg);
	}
}

void work_init()
{
	readpos = 0;
	writepos = 0;
	pending_work = 0;

	// spawn working threads
	mutex_create(&mutex);
	condvar_create(&cond);
	running = 1;
	thread_count = MAX(1, MIN(MAX_THREADS, sys_get_cpu_count() - 1));
	for(long i = 0; i < thread_count; i++){
		if(thread_create(&thread_pool[i], worker_thread, NULL))
			LOG_ERROR("work_init: failed to spawn worker thread #%d", i);
	}
}

void work_shutdown()
{
	// join threads
	mutex_lock(mutex);
	running = 0;
	condvar_broadcast(cond);
	mutex_unlock(mutex);

	for(long i = 0; i < thread_count; i++){
		thread_join(thread_pool[i]);
		thread_release(thread_pool[i]);
	}

	condvar_destroy(cond);
	mutex_destroy(mutex);
}

void work_dispatch(void (*fp)(void *), void *arg)
{
	//DEBUG_CHECK(running == 0, "work_dispatch: worker threads are not running");
	if(running == 0){
		LOG_ERROR("work_dispatch: worker threads not running");
		return;
	}

	mutex_lock(mutex);
	if(pending_work >= MAX_WORK){
		LOG_ERROR("work_dispatch: work ring buffer is at maximum capacity (%d)", MAX_WORK);
		mutex_unlock(mutex);
		return;
	}

	work_pool[writepos].fp = fp;
	work_pool[writepos].arg = arg;
	++writepos;
	if(writepos >= MAX_WORK)
		writepos = 0;
	++pending_work;
	condvar_signal(cond);
	mutex_unlock(mutex);
}
