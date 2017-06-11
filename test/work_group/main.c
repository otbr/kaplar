#include "../../src/log.h"
#include "../../src/work.h"
#include "../../src/work_group.h"
#include "../../src/system.h"

#include <stdlib.h>
#include <stdio.h>

typedef volatile int atomic_int;

static long start_tick = 0;
static void complete(void *unused)
{
	(void)unused;
	LOG("complete: %ld", sys_get_tick_count() - start_tick);
}
static void test0(void *unused)
{
	(void)unused;
	LOG("test0: %ld", sys_get_tick_count() - start_tick);
}
static void test1(void *unused)
{
	(void)unused;
	LOG("test1: %ld", sys_get_tick_count() - start_tick);
}
int main(int argc, char **argv)
{
	struct work_group *grp;

	// init worker
	work_init();

	// create work group
	grp = work_group_create();
	for(int i = 0; i < 10; i++){
		work_group_add(grp, test0, NULL);
		work_group_add(grp, test1, NULL);
	}

	// run first test
	LOG("work_group_dispatch(grp=%p, fp=%p, arg=%p)", grp, NULL, NULL);
	start_tick = sys_get_tick_count();
	work_group_dispatch(grp, NULL, NULL);
	getchar();

	// run second test
	LOG("work_group_dispatch(grp=%p, fp=%p, arg=%p)", grp, complete, NULL);
	start_tick = sys_get_tick_count();
	work_group_dispatch(grp, complete, NULL);
	getchar();

	// run third test
	LOG("work_group_dispatch_array(grp = %p)", grp);
	start_tick = sys_get_tick_count();
	work_group_dispatch_array(grp);
	getchar();

	// release work group
	work_group_release(grp);

	// cleanup
	work_shutdown();
	return 0;
}
