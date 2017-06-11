#include "atomic.h"
#include "log.h"
#include "work.h"

#include <stdlib.h>
#include <stddef.h>

#define GROUP_MAX_WORK 32
struct work_group{
	struct work	complete;
	struct work	work[GROUP_MAX_WORK];
	long		work_count;
	atomic_int	idx;
	atomic_int	counter;
};

static void work_group_complete(void *arg)
{
	int idx;
	struct work_group *grp = arg;
	if(grp == NULL)
		return;
	idx = atomic_xadd(&grp->idx, 1);
	grp->work[idx].fp(grp->work[idx].arg);

	// if the counter reaches zero, all work
	// has been completed and the complete
	// routine may be called
	if(atomic_xsub(&grp->counter, 1) <= 1)
		grp->complete.fp(grp->complete.arg);
}


struct work_group *work_group_create()
{
	struct work_group *grp;

	// TODO: use general allocator
	grp = malloc(sizeof(struct work_group));

	grp->work_count = 0;
	return grp;
}

void work_group_release(struct work_group *grp)
{
	// TODO: use general allocator
	free(grp);
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
	struct work work = {
		.fp = work_group_complete,
		.arg = grp,
	};

	if(fp == NULL){
		LOG_ERROR("work_group_dispatch: the complete routine must be valid!");
		return;
	}
	grp->complete.fp = fp;
	grp->complete.arg = arg;
	grp->counter = 0;
	work_dispatch_array(grp->work_count, 1, &work);
}

void work_group_dispatch_array(struct work_group *grp)
{
	work_dispatch_array(grp->work_count, 0, grp->work);
}
