#include "scheduler.h"

#include "work.h"
#include "array.h"
#include "types.h"
#include "thread.h"
#include "log.h"
#include "system.h"

#include <stddef.h>

struct sch_entry{
	long time;
	void (*fp)(void *);
	void *arg;

	struct sch_entry *next;
};


// entry list
#define MAX_LIST_SIZE 1024
static struct array *sch_array;
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
			if(head == NULL || head->time > sys_get_tick_count()){
				mutex_unlock(mtx);
				continue;
			}
		}

		// remove head
		fp = head->fp;
		arg = head->arg;
		tmp = head;
		head = head->next;
		array_del(sch_array, tmp);
		mutex_unlock(mtx);

		// dispatch task to the worker threads
		work_dispatch(fp, arg);
	}
}

void scheduler_init()
{
	// create memory pool for the entry list
	sch_array = array_create(MAX_LIST_SIZE, sizeof(struct sch_entry));
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
	array_destroy(sch_array);
}

struct sch_entry *scheduler_add(long delay, void (*fp)(void *), void *arg)
{
	struct sch_entry *entry, **it;

	if(running == 0){
		LOG_ERROR("scheduler_add: scheduler thread not running");
		return NULL;
	}

	mutex_lock(mtx);
	// retrieve memory for the entry
	entry = array_new(sch_array);
	if(entry == NULL){
		LOG_ERROR("scheduler_add: entry list is at maximum capacity (%d)", MAX_LIST_SIZE);
		mutex_unlock(mtx);
		return NULL;
	}
	entry->time = sys_get_tick_count() + delay;
	entry->fp = fp;
	entry->arg = arg;
	// sort list by time
	it = &head;
	while(*it != NULL && (*it)->time <= entry->time)
		it = &(*it)->next;
	entry->next = *it;
	*it = entry;
	// signal scheduler if the head has changed
	if(entry == head)
		condvar_signal(cond);
	mutex_unlock(mtx);
	return entry;
}

void scheduler_remove(struct sch_entry *entry)
{
	struct sch_entry **it;

	mutex_lock(mtx);
	it = &head;
	while(*it != NULL && *it != entry)
		it = &(*it)->next;

	if(*it != NULL && *it == entry){
		*it = (*it)->next;
		array_del(sch_array, entry);
	}
	else{
		LOG_WARNING("scheduler_remove: trying to remove invalid entry");
	}
	mutex_unlock(mtx);
}

void scheduler_reschedule(long delay, struct sch_entry *entry)
{
	long time;
	struct sch_entry **it;

	mutex_lock(mtx);
	// retrieve entry from list
	it = &head;
	while(*it != NULL && *it != entry)
		it = &(*it)->next;

	// check the entry is valid
	if(*it != NULL && *it == entry){
		// remove entry from current position
		*it = (*it)->next;

		// check if we need to restart iteration
		time = sys_get_tick_count() + delay;
		if((*it)->time > time)
			it = &head;

		// get new position
		while(*it != NULL && (*it)->time <= time)
			it = &(*it)->next;
		// re-insert entry on new position
		entry->time = time;
		entry->next = *it;
		*it = entry;

		// signal scheduler if the head has changed
		if(*it == head)
			condvar_signal(cond);
	}
	else{
		LOG_WARNING("scheduler_reschedule: trying to reschedule invalid entry");
	}
	mutex_unlock(mtx);
}

void scheduler_pop(struct sch_entry *entry)
{
	struct sch_entry **it;

	mutex_lock(mtx);
	// retrieve entry from list
	it = &head;
	while(*it != NULL && (*it) != entry)
		it = &(*it)->next;

	if(*it != NULL){
		// remove it from current position and
		// put it on top of the list
		*it = (*it)->next;
		entry->time = 0;
		entry->next = head;
		head = entry;

		// signal scheduler that the head has changed
		condvar_signal(cond);
	}
	else{
		LOG_WARNING("scheduler_pop: trying to pop invalid entry");
	}
	mutex_unlock(mtx);
}
