#include "../system.h"

#include <time.h>
#include <unistd.h>

long sys_get_tick_count(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (ts.tv_sec * 1000) +
		(ts.tv_nsec / 1000000);
}

long sys_get_cpu_count(void)
{
	// this option is available on FreeBSD
	// since version 5.0
	return sysconf(_SC_NPROCESSORS_ONLN);
}
