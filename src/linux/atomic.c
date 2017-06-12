#include "../atomic.h"

#if defined(__x86_64__) || defined(__x86_64)		\
	|| defined(__amd64__) || defined(__amd64)	\
	|| defined(i386) || defined(__i386)		\
	|| defined(__i386__) || defined(__i486__)	\
	|| defined(__i586__) || defined(__i686__)	\
	|| defined(__X86__) || defined(_X86_)


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
	__asm__ __volatile__(
		"lock"			"\n\t"
		"addl %%eax, %0"	"\n\t"
		: "+m"(*x)
		: "a"(val)
		:);
}

int atomic_fetch_add(atomic_int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"xaddl %%eax, %0"	"\n\t"
		: "+m"(*x), "+a"(val)
		:
		:);
	return val;
}

int atomic_exchange(atomic_int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"xchgl %%edx, %0"	"\n\t"
		: "+m"(*x), "+d"(val)
		:
		:);
	return val;
}

int atomic_compare_exchange(atomic_int *x, int cmp, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"cmpxchgl %%edx, %0"	"\n\t"
		: "+m"(*x), "+a"(cmp)
		: "d"(val)
		:);
	return cmp;
}

void atomic_lwfence()
{
	// this may not be optimal
	__asm__ __volatile__(
		"" ::: "memory");
}

void atomic_hwfence()
{
	__asm__ __volatile__(
		"mfence" ::: "memory");
}

#else
#error "Atomics not supported on this architecture"
#endif

