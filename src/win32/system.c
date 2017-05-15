#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

long sys_get_tick_count()
{
	return GetTickCount();
}

long sys_get_cpu_count()
{
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}
