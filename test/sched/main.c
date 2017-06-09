#include "../../src/log.h"
#include "../../src/scheduler.h"
#include "../../src/work.h"
#include "../../src/system.h"

#include <stdlib.h>
#include <stdio.h>

static long start_tick = 0;

static void test(void *unused)
{
	(void)unused;
	LOG("test: %ld", sys_get_tick_count() - start_tick);
}

int main(int argc, char **argv)
{
	int i;

	// init scheduler
	work_init();
	scheduler_init();

	start_tick = sys_get_tick_count();
	for(i = 1; i <= 10; i++)
		scheduler_add(i*1000, test, NULL);

	getchar();
	// cleanup
	work_shutdown();
	scheduler_shutdown();
	return 0;
}
