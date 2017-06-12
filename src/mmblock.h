#ifndef MMBLOCK_H_
#define MMBLOCK_H_

struct mmblock;

struct mmblock *mmblock_create(long slots, long stride);
void mmblock_release(struct mmblock *blk);
void *mmblock_alloc(struct mmblock *blk);
void mmblock_free(struct mmblock *blk, void *ptr);
void mmblock_init_lock(struct mmblock *blk);
void *mmblock_xalloc(struct mmblock *blk);
void mmblock_xfree(struct mmblock *blk, void *ptr);
void mmblock_report(struct mmblock *blk);

#endif //MMBLOCK_H_
