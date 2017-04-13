#include "array.h"
#include "log.h"

#include <stdlib.h>
#include <stdio.h>

struct array{
	long stride;
	long capacity;
	long offset;
	void *base;
	void *freelist;
};

struct array *array_create(long slots, long stride)
{
	long capacity, offset;
	struct array *array;

	// properly align stride and data offset to have aligned allocations
	const static unsigned mask = sizeof(void*) - 1;
	if((stride & mask) != 0)
		stride = (stride + mask) & ~mask;

	offset = sizeof(struct array);
	if((offset & mask) != 0)
		offset = (offset + mask) & ~mask;

	capacity = slots * stride;
	array = malloc(offset + capacity);
	array->stride = stride;
	array->capacity = capacity;
	array->offset = 0;
	array->base = (char*)(array) + offset;
	array->freelist = NULL;
	return array;
}

void array_destroy(struct array *array)
{
	free(array);
}

void *array_new(struct array *array)
{
	void *ptr = NULL;

	if(array->freelist != NULL){
		ptr = array->freelist;
		array->freelist = *(void**)(array->freelist);
		return ptr;
	}

	if(array->offset < array->capacity){
		ptr = (char*)array->base + (array->offset);
		array->offset += array->stride;
	}

	return ptr;
}

void array_del(struct array *array, void *ptr)
{
	char *base = array->base;

	// check if ptr was allocated by this allocator
	if(ptr < base || ptr >= (base + array->capacity))
		return;

	// return memory to allocator
	if(ptr == (base + array->offset - array->stride)){
		array->offset -= array->stride;
	}
	else{
		*(void**)(ptr) = array->freelist;
		array->freelist = ptr;
	}
}

void *array_get(struct array *array, long idx)
{
	void *it;
	char *base = array->base;
	void *ptr = base + (idx * array->stride);

	// check if it's in range
	if(ptr < base || ptr >= (base + array->capacity))
		return NULL;

	// check if it's not on the freelist
	for(it = array->freelist; it != NULL; it = *(void**)it){
		if(it == ptr)
			return NULL;
	}
	return ptr;
}

void array_report(struct array *array)
{
	void *it;
	LOG("array report:");
	LOG("\tstride = %ld", array->stride);
	LOG("\tcapacity = %ld", array->capacity);
	LOG("\toffset = %ld", array->offset);
	LOG("\tbase: %p", array->base);
	LOG("\tfreelist:");
	for(it = array->freelist; it != NULL; it = *(void**)it)
		LOG("\t\t* %p", it);
}