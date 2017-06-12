#ifndef MM_H_
#define MM_H_

struct mmblock;
void mm_init(void);
void mm_shutdown(void);
int mm_add_block(struct mmblock *blk);
void *mm_alloc(long size);
void mm_free(void *ptr);

#endif //MM_H_
