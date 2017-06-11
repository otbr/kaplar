#ifndef ATOMIC_H_
#define ATOMIC_H_

// atomic integer type
// NOTE: it is declared as volatile to prevent
// the compiler from changing the operation
// order on the optimizer stage
typedef volatile int atomic_int;

#if defined(__x86_64__) || defined(__x86_64)		\
	|| defined(__amd64__) || defined(__amd64)	\
	|| defined(i386) || defined(__i386)		\
	|| defined(__i386__) || defined(__i486__)	\
	|| defined(__i586__) || defined(__i686__)	\
	|| defined(__X86__) || defined(_X86_)


static inline int
atomic_load(atomic_int *x)
{
	return *x;
}

static inline void
atomic_store(atomic_int *x, int val)
{
	*x = val;
}

static inline void
atomic_add(atomic_int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"addl %%eax, %0"	"\n\t"
		: "+m"(*x)
		: "a"(val)
		:);
}

static inline int
atomic_xadd(atomic_int *x, int val)
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
atomic_sub(atomic_int *x, int val)
{
	__asm__ __volatile__(
		"lock"			"\n\t"
		"sub %%eax, %0"		"\n\t"
		: "+m"(*x)
		: "a"(val)
		:);
}

static inline int
atomic_xsub(atomic_int *x, int val)
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
atomic_xchg(atomic_int *x, int val)
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
atomic_cmpxchg(atomic_int *x, int cmp, int val)
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
