#ifndef ATOMIC_H_
#define ATOMIC_H_

#if defined(__x86_64__) || defined(__x86_64)		\
	|| defined(__amd64__) || defined(__amd64)	\
	|| defined(i386) || defined(__i386)		\
	|| defined(__i386__) || defined(__i486__)	\
	|| defined(__i586__) || defined(__i686__)	\
	|| defined(__X86__) || defined(_X86_)

static inline int
atomic_load(int *x)
{
	return *x;
}

static inline void
atomic_store(int *x, int val)
{
	*x = val;
}

static inline void
atomic_add(int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"addl %%eax, %0"	"\n\t"
		: "+m"(*x)
		: "a"(val)
		:);
}

static inline int
atomic_xadd(int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"xaddl %%eax, %0"	"\n\t"
		: "+m"(*x), "+a"(val)
		:
		:);
	return val;
}


static inline void
atomic_sub(int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"sub %%eax, %0"		"\n\t"
		: "+m"(*x)
		: "a"(val)
		:);
}

static inline int
atomic_xsub(int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"xadd %%eax, %0"	"\n\t"
		: "+m"(*x), "+a"(val)
		: "a"(-val)
		:);
	return val;
}

static inline int
atomic_xchg(int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"xchgl %%edx, %0"	"\n\t"
		: "+m"(*x), "+d"(val)
		:
		:);
	return val;
}

static inline int
atomic_cmpxchg(int *x, int cmp, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"cmpxchgl %%edx, %0"	"\n\t"
		: "+m"(*x), "=a"(val)
		: "a"(cmp), "d"(val)
		:);
	return val;
}

static inline void
atomic_lwfence()
{
	// this may not be optimal
	__asm__ __volatile__(
		"" ::: "memory");
}

static inline void
atomic_hwfence()
{
	__asm__ __volatile__(
		"mfence" ::: "memory");
}

#else
#error "Atomics not supported on this architecture"
#endif

#endif //ATOMIC_H_
