#include "scheduler.h"

#include "work.h"
#include "mem.h"
#include "types.h"
#include "thread.h"
#include "log.h"
#include "system.h"

struct sch_entry{
	long time;
	void (*fp)(void *);
	void *arg;

	struct sch_entry *next;
};


// entry list
#define MAX_LIST_SIZE 1024
static struct mem_pool *sch_mem;
static struct sch_entry *head;
static long entry_count;

// scheduler thread
static struct thread *thread;
static struct mutex *mtx;
static struct condvar *cond;
static int running = 0;

static void scheduler(void *unused)
{
	struct sch_entry *tmp;
	long delta;
	void (*fp)(void *);
	void *arg;

	while(running != 0){
		mutex_lock(mtx);
		// check if there is queued work
		if(head == NULL){
			condvar_wait(cond, mtx);
			if(head == NULL){
				mutex_unlock(mtx);
				continue;
			}
		}

		// check if it's time for execution
		delta = head->time - sys_get_tick_count();
		if(delta > 0){
			condvar_timedwait(cond, mtx, delta);
			if(head == NULL || head->time < sys_get_tick_count()){
				mutex_unlock(mtx);
				continue;
			}
		}

		// remove head
		fp = head->fp;
		arg = head->arg;
		tmp = head;
		head = head->next;
		mem_pool_free(sch_mem, tmp);
		mutex_unlock(mtx);

		// dispatch task to the worker threads
		work_dispatch(fp, arg);
	}
}

void scheduler_init()
{
	// create memory pool for the entry list
	mem_create_pool(MAX_LIST_SIZE, sizeof(struct sch_entry), &sch_mem);
	head = NULL;
	entry_count = 0;

	// spawn scheduler thread
	mutex_create(&mtx);
	condvar_create(&cond);
	running = 1;
	if(thread_create(&thread, scheduler, NULL) != 0)
		LOG_ERROR("scheduler_init: failed to spawn scheduler thread");
}

void scheduler_shutdown()
{
	// join scheduler thread
	mutex_lock(mtx);
	running = 0;
	condvar_broadcast(cond);
	mutex_unlock(mtx);

	thread_join(thread);
	thread_release(thread);

	// release resources
	condvar_destroy(cond);
	mutex_destroy(mtx);

	// release memory pool
	mem_destroy_pool(sch_mem);
}

struct sch_entry *scheduler_add(long delay, void (*fp)(void *), void *arg)
{
	long time;
	struct sch_entry *entry, **it;

	if(running == 0){
		LOG_ERROR("scheduler_add: scheduler thread not running");
		return NULL;
	}

	time = sys_get_tick_count() + delay;
	mutex_lock(mtx);
	// retrieve memory for the entry
	entry = mem_pool_alloc(sch_mem);
	if(entry == NULL){
		LOG_ERROR("scheduler_add: entry list is at maximum capacity (%d)", MAX_LIST_SIZE);
		mutex_unlock(mtx);
		return NULL;
	}
	entry->time = time;
	entry->fp = fp;
	entry->arg = arg;
	// sort list by time
	it = &head;
	while(*it != NULL && (*it)->time <= time)
		it = &(*it)->next;
	entry->next = *it;
	*it = entry;
	// signal thread if the head has changed
	if(entry == head)
		condvar_signal(cond);
	mutex_unlock(mtx);
	return entry;
}

void scheduler_rem(struct sch_entry *entry)
{
	struct sch_entry **it;

	mutex_lock(mtx);
	it = &head;
	while(*it != NULL && *it != entry)
		it = &(*it)->next;

	if(*it != NULL && *it == entry){
		*it = (*it)->next;
		mem_pool_free(sch_mem, entry);
	}
	else{
		LOG_WARNING("scheduler_rem: trying to remove invalid entry");
	}
	mutex_unlock(mtx);
}