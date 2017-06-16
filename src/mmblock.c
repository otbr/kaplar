#include "mmblock.h"
#include "log.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>

struct mmblock{
	long stride;
	long capacity;
	long offset;
	void *base;
	void *freelist;
	struct mutex *lock;
};

struct mmblock *mmblock_create(long slots, long stride)
{
	long capacity, offset;
	struct mmblock *blk;

	// properly align stride and data offset to have aligned allocations
	static const unsigned mask = sizeof(void*) - 1;
	if((stride & mask) != 0)
		stride = (stride + mask) & ~mask;

	offset = sizeof(struct mmblock);
	if((offset & mask) != 0)
		offset = (offset + mask) & ~mask;

	capacity = slots * stride;
	blk = malloc(offset + capacity);
	if(blk == NULL){
		LOG_ERROR("memory_block_create: out of memory");
		return NULL;
	}
	blk->stride = stride;
	blk->capacity = capacity;
	blk->offset = 0;
	blk->base = (char*)(blk) + offset;
	blk->freelist = NULL;
	blk->lock = NULL;
	return blk;
}

void mmblock_release(struct mmblock *blk)
{
	if(blk->lock != NULL)
		mutex_destroy(blk->lock);
	free(blk);
}

void *mmblock_alloc(struct mmblock *blk)
{
	void *ptr = NULL;
	if(blk->freelist != NULL){
		ptr = blk->freelist;
		blk->freelist = *(void**)(blk->freelist);
		return ptr;
	}

	if(blk->offset < blk->capacity){
		ptr = (char*)blk->base + blk->offset;
		blk->offset += blk->stride;
	}

	return ptr;
}

void mmblock_free(struct mmblock *blk, void *ptr)
{
	// check if ptr belongs to this block
	if(ptr < blk->base || ptr >= (void*)((char*)blk->base + blk->capacity))
		return;

	// return memory to block
	if(ptr == (void*)((char*)blk->base + blk->offset - blk->stride)){
		blk->offset -= blk->stride;
	}
	else{
		*(void**)(ptr) = blk->freelist;
		blk->freelist = ptr;
	}
}

int mmblock_contains(struct mmblock *blk, void *ptr)
{
	if(ptr >= blk->base && ptr < (void*)((char*)blk->base + blk->capacity))
		return 0;
	return -1;
}

void mmblock_init_lock(struct mmblock *blk)
{
	if(blk->lock != NULL)
		LOG_WARNING("mmblock_init_lock: lock already initialized");
	mutex_create(&blk->lock);
}

void *mmblock_xalloc(struct mmblock *blk)
{
	void *ptr;
	mutex_lock(blk->lock);
	ptr = mmblock_alloc(blk);
	mutex_unlock(blk->lock);
	return ptr;
}

void mmblock_xfree(struct mmblock *blk, void *ptr)
{
	mutex_lock(blk->lock);
	mmblock_free(blk, ptr);
	mutex_unlock(blk->lock);
}

void mmblock_report(struct mmblock *blk)
{
	void *ptr;
	LOG("memory block report:");
	LOG("\tstride = %ld", blk->stride);
	LOG("\tcapacity = %ld", blk->capacity);
	LOG("\toffset = %ld", blk->offset);
	LOG("\tbase: %p", blk->base);
	LOG("\tfreelist:");
	for(ptr = blk->freelist; ptr != NULL; ptr = *(void**)ptr)
		LOG("\t\t* %p", ptr);
}
