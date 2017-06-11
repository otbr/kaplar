#include "work.h"

#include "thread.h"
#include "system.h"
#include "log.h"
#include "util.h"

#include <stddef.h>

// work ring buffer
#define MAX_WORK 4096
static struct work work_pool[MAX_WORK];
static int readpos;
static int writepos;
static int pending_work;

// thread pool
#define MAX_THREADS 64
static struct thread *thread_pool[MAX_THREADS];
static struct mutex *lock;
static struct condvar *cond;
static int thread_count;
static int running = 0;


static void worker_thread(void *unused)
{
	void (*fp)(void*);
	void *arg;
	//struct work_group *grp;
	while(running != 0){
		// retrieve work
		mutex_lock(lock);
		if(pending_work <= 0){
			condvar_wait(cond, lock);
			if(pending_work <= 0){
				mutex_unlock(lock);
				continue;
			}
		}
		fp = work_pool[readpos].fp;
		arg = work_pool[readpos].arg;
		//grp = work_pool[readpos].grp;
		++readpos;
		if(readpos >= MAX_WORK)
			readpos = 0;
		--pending_work;
		mutex_unlock(lock);

		// execute work
		fp(arg);

		// if(grp != NULL)
		//	work_group_complete(grp);
	}
}

void work_init()
{
	readpos = 0;
	writepos = 0;
	pending_work = 0;

	// spawn working threads
	mutex_create(&lock);
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
	mutex_lock(lock);
	running = 0;
	condvar_broadcast(cond);
	mutex_unlock(lock);

	for(long i = 0; i < thread_count; i++){
		thread_join(thread_pool[i]);
		thread_release(thread_pool[i]);
	}

	condvar_destroy(cond);
	mutex_destroy(lock);
}

void work_dispatch(void (*fp)(void*), void *arg)
{
	if(running == 0){
		LOG_ERROR("work_dispatch: worker threads not running");
		return;
	}

	mutex_lock(lock);
	if(pending_work >= MAX_WORK){
		LOG_ERROR("work_dispatch: work ring buffer is at maximum capacity (%d)", MAX_WORK);
		mutex_unlock(lock);
		return;
	}

	++pending_work;
	work_pool[writepos].fp = fp;
	work_pool[writepos].arg = arg;
	++writepos;
	if(writepos >= MAX_WORK)
		writepos = 0;
	condvar_signal(cond);
	mutex_unlock(lock);
}


void work_dispatch_array(int count, int single, struct work *work)
{
	if(running == 0){
		LOG_ERROR("work_dispatch_array: worker threads not running");
		return;
	}

	mutex_lock(lock);
	if(pending_work + count >= MAX_WORK){
		LOG_ERROR("work_dispatch_array: requested amount of work would case the ring buffer to overflow");
		mutex_unlock(lock);
		return;
	}

	pending_work += count;
	for(int i = 0; i < count; i++){
		work_pool[writepos].fp = work->fp;
		work_pool[writepos].arg = work->arg;
		// if there is a single work in the array
		// keep adding it to the work pool, else
		// advance to the next element
		if(single == 0)
			work++;

		writepos++;
		if(writepos >= MAX_WORK)
			writepos = 0;
	}
	condvar_broadcast(cond);
	mutex_unlock(lock);
}

