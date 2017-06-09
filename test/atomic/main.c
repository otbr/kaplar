#include "../../src/log.h"
#include "../../src/thread.h"
#include "../../src/atomic.h"

#include <stdlib.h>
#include <stdio.h>

static int a_counter	= 0;
static int counter	= 0;
static void test(void *unused)
{
	int i;

	(void)unused;
	for(i = 0; i < 1000; i++){
		atomic_add(&a_counter, 1);
		counter++;
	}
}

int main(int argc, char **argv)
{
	int i;
	struct thread *thr[10];
	for(i = 0; i < 10; i++)
		thread_create(&thr[i], test, NULL);
	for(i = 0; i < 10; i++){
		thread_join(thr[i]);
		thread_release(thr[i]);
	}
	LOG("atomic counter = %d", a_counter);
	LOG("non-atomic counter = %d", counter);
	return 0;
}
