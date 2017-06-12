#include "../../src/log.h"
#include "../../src/system.h"
#include "../../src/mm.h"

#include <stdlib.h>
#include <stdio.h>

static long start_tick = 0;
int main(int argc, char **argv)
{
	// init allocator
	mm_init();


	void *ptr1[64], *ptr2[64];
	long size, i;
	for(i = 0; i < 64; i++){
		size = (i + 1) * 16;
		ptr1[i] = mm_alloc(size);
		LOG("ptr1(%d) = %p", size, ptr1[i]);
	}
	for(i = 0; i < 64; i++){
		size = (i + 1) * 16;
		ptr2[i] = mm_alloc(size);
		LOG("ptr2(%d) = %p", size, ptr2[i]);
	}

	start_tick = sys_get_tick_count();
	for(i = 0; i < 64; i++){
		mm_free(ptr1[i]);
		LOG("free: ptr1 = %p, tick = %ld", ptr1[i], sys_get_tick_count() - start_tick);
	}
	for(i = 0; i < 64; i++){
		mm_free(ptr2[i]);
		LOG("free: ptr2 = %p, tick = %ld", ptr2[i], sys_get_tick_count() - start_tick);
	}

	// cleanup
	mm_shutdown();
	return 0;
}
