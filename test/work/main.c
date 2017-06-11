#include "../../src/log.h"
#include "../../src/work.h"
#include "../../src/system.h"

#include <stdlib.h>
#include <stdio.h>

static long start_tick = 0;
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
	struct work work[] = {
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
		{test1, NULL},
		{test0, NULL},
	};

	// init worker
	work_init();

	// run first test
	LOG("work_dispatch_array(count=15, single=0, work=%p)", work);
	start_tick = sys_get_tick_count();
	work_dispatch_array(15, 0, work);
	getchar();

	// run second test
	LOG("work_dispatch_array(count=15, single=1, work=%p)", work);
	start_tick = sys_get_tick_count();
	work_dispatch_array(15, 1, work);
	getchar();

	// cleanup
	work_shutdown();
	return 0;
}
