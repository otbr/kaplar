#ifndef __MEM_H__
#define __MEM_H__

struct mem_pool;

void mem_create_pool(long slots, long stride, struct mem_pool **pool);
void mem_destroy_pool(struct mem_pool *pool);

void *mem_pool_alloc(struct mem_pool *pool);
void mem_pool_free(struct mem_pool *pool, void *ptr);
void mem_pool_report(struct mem_pool *pool);

#endif //__MEM_H__