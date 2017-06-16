#include "atomic.h"
#include "log.h"
#include "work.h"
#include "mm.h"

#include <stdlib.h>
#include <stddef.h>

#define GROUP_MAX_WORK 32
struct work_group{
	struct work	complete;
	struct work	work[GROUP_MAX_WORK];
	long		work_count;

	atomic_int	next_work;
	atomic_int	work_left;
};

static void work_group_complete(void *arg)
{
	int cur, work_left;
	struct work_group *grp = arg;
	if(grp == NULL)
		return;
	atomic_lwfence();
	cur = atomic_fetch_add(&grp->next_work, 1);
	atomic_lwfence();
	grp->work[cur].fp(grp->work[cur].arg);
	work_left = atomic_fetch_add(&grp->work_left, -1);
	atomic_lwfence();
	if(work_left <= 1)
		grp->complete.fp(grp->complete.arg);
}


struct work_group *work_group_create()
{
	struct work_group *grp;

	grp = mm_alloc(sizeof(struct work_group));
	grp->work_count = 0;
	return grp;
}

void work_group_release(struct work_group *grp)
{
	mm_free(grp);
}

void work_group_add(struct work_group *grp, void (*fp)(void*), void *arg)
{
	if(grp->work_count >= GROUP_MAX_WORK){
		LOG_ERROR("work_group_add: group has reached maximum amount of work (%d)", GROUP_MAX_WORK);
		return;
	}
	grp->work[grp->work_count].fp = fp;
	grp->work[grp->work_count].arg = arg;
	grp->work_count += 1;
}


void work_group_dispatch(struct work_group *grp, void (*fp)(void*), void *arg)
{
	struct work work = {work_group_complete, grp};

	if(fp == NULL){
		LOG_ERROR("work_group_dispatch: the complete routine must be valid!");
		return;
	}
	grp->complete.fp = fp;
	grp->complete.arg = arg;
	grp->next_work = 0;
	grp->work_left = grp->work_count;
	work_dispatch_array(grp->work_count, 1, &work);
}

void work_group_dispatch_array(struct work_group *grp)
{
	work_dispatch_array(grp->work_count, 0, grp->work);
}
