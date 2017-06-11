#ifndef ATOMIC_H_
#define ATOMIC_H_

// atomic integer type
// NOTE: it is declared as volatile to prevent
// the compiler from changing the operation
// order on the optimizer stage
typedef volatile int atomic_int;

int	atomic_load(atomic_int *x);
void	atomic_store(atomic_int *x, int val);
void	atomic_add(atomic_int *x, int val);
int	atomic_fetch_add(atomic_int *x, int val);
int	atomic_exchange(atomic_int *x, int val);
int	atomic_compare_exchange(atomic_int *x, int cmp, int val);
void	atomic_lwfence();
void	atomic_hwfence();

#endif //ATOMIC_H_
