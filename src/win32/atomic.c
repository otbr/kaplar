#include "../atomic.h"

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>

int atomic_load(atomic_int *x)
{
	return *x;
}

void atomic_store(atomic_int *x, int val)
{
	*x = val;
}

void atomic_add(atomic_int *x, int val)
{
	InterlockedAdd(x, val);
}

int atomic_fetch_add(atomic_int *x, int val)
{
	return InterlockedExchangeAdd(x, val);
}

int atomic_exchange(atomic_int *x, int val)
{
	return InterlockedExchange(x, val);
}

int atomic_compare_exchange(atomic_int *x, int cmp, int val)
{
	return InterlockedCompareExchange(x, val, cmp);
}

// I think the Interlocked* API doesn't need any kind of fence
void atomic_lwfence()
{
	//MemoryBarrier();
}

void atomic_hwfence()
{
	//MemoryBarrier();
}