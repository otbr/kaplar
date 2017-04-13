#include "mem.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>

struct mem_pool{
	long stride;
	long capacity;
	long offset;
	void *base;
	void *freelist;
};

void mem_create_pool(long slots, long stride, struct mem_pool **pool)
{
	long capacity, offset;

	// properly align stride and data offset to have aligned allocations
	const static unsigned mask = sizeof(void*) - 1;
	if((stride & mask) != 0)
		stride = (stride + mask) & ~mask;

	offset = sizeof(struct mem_pool);
	if((offset & mask) != 0)
		offset = (offset + mask) & ~mask;

	capacity = slots * stride;
	(*pool) = malloc(offset + capacity);
	(*pool)->stride = stride;
	(*pool)->capacity = capacity;
	(*pool)->offset = 0;
	(*pool)->base = (char *)(*pool) + offset;
	(*pool)->freelist = NULL;
}

void mem_destroy_pool(struct mem_pool *pool)
{
	free(pool);
}

void *mem_pool_alloc(struct mem_pool *pool)
{
	void *ptr = NULL;

	if(pool->freelist != NULL){
		ptr = pool->freelist;
		pool->freelist = *(void**)(pool->freelist);
		return ptr;
	}

	if(pool->offset < pool->capacity){
		ptr = (char*)pool->base + (pool->offset);
		pool->offset += pool->stride;
	}

	return ptr;
}

void mem_pool_free(struct mem_pool *pool, void *ptr)
{
	char *base = pool->base;

	// check if ptr was allocated by this allocator
	if(!(ptr >= base && ptr < (base + pool->capacity)))
		return;

	// return memory to allocator
	if(ptr == base + pool->offset - pool->stride){
		pool->offset -= pool->stride;
	}
	else{
		*(void**)(ptr) = pool->freelist;
		pool->freelist = ptr;
	}
}

void mem_pool_report(struct mem_pool *pool)
{
	void *it;
	LOG("memory pool report:");
	LOG("\tstride = %ld", pool->stride);
	LOG("\tcapacity = %ld", pool->capacity);
	LOG("\toffset = %ld", pool->offset);
	LOG("\tbase: %p", pool->base);
	LOG("\tfreelist:");
	for(it = pool->freelist; it != NULL; it = *(void**)it)
		LOG("\t\t* %p", it);
}