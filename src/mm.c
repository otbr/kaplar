#include "mm.h"
#include "mmblock.h"
#include "thread.h"
#include "log.h"
#include <stddef.h>

// blocks created by the general allocator
// may have 64 slots max so it doesn't end
// up eating all memory
#define BLK_MAX_SLOTS 64

// every block created by the general allocator
// will be a multiple of 16
#define ROUND_TO_16(x) (((x) + 15) & ~15)

// stride and capacity are at the beggining
// of the mmblock structure and as we're not
// gonna use the other fields, we may just
// declare these
struct mmblock{
	long stride;
	long capacity;
};

#define MAX_BLOCKS 256
static struct mutex	*lock;
static struct mmblock	*blk_list[MAX_BLOCKS];
static long		blk_count;

void mm_init(void)
{
	blk_count = 0;
	mutex_create(&lock);
}

void mm_shutdown(void)
{
	mutex_destroy(lock);
}

int mm_add_block(struct mmblock *blk)
{
	int i, j;

	if(blk_count >= MAX_BLOCKS) return -1;
	for(i = 0; i < blk_count && blk_list[i]->stride < blk->stride; i++);
	for(j = i; j < blk_count; j++)
		blk_list[j+1] = blk_list[j];
	blk_count++;
	blk_list[i] = blk;
	return 0;
}

void *mm_alloc(long size)
{
	void *ptr;
	struct mmblock *blk;
	long size16, i;

	size16 = ROUND_TO_16(size);
	mutex_lock(lock);
	for(i = 0; i < blk_count && blk_list[i]->stride < size16; i++);
	blk = blk_list[i];
	if(blk_count == 0)
		blk = NULL;

	// if there is a block big enough but surpasses
	// the size rounded to 16, create another block
	if(blk != NULL && blk->stride != size16)
		blk = NULL;

	// allocate now if there is a block
	if(blk != NULL){
		ptr = mmblock_alloc(blk);
		if(ptr != NULL){
			mutex_unlock(lock);
			return ptr;
		}
	}

	// create block and allocate otherwise
	blk = mmblock_create(BLK_MAX_SLOTS, size16);
	if(mm_add_block(blk) == -1){
		mutex_unlock(lock);
		LOG_ERROR("mm_alloc: reached maximum number of memory blocks (%d)", MAX_BLOCKS);
		mmblock_release(blk);
		return NULL;
	}

	ptr = mmblock_alloc(blk);
	mutex_unlock(lock);
	return ptr;
}

void mm_free(void *ptr)
{
	// since there is no information about
	// the block, just call free from every
	// block in the list
	mutex_lock(lock);
	for(int i = 0; i < blk_count; i++)
		mmblock_free(blk_list[i], ptr);
	mutex_unlock(lock);
}
