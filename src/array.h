#ifndef __ARRAY_H__
#define __ARRAY_H__

struct array;

struct array *array_create(long slots, long stride);
void array_destroy(struct array *array);

void *array_new(struct array *array);
void array_del(struct array *array, void *ptr);
void *array_get(struct array *array, long idx);

void array_report(struct array *array);

#endif //__ARRAY_H__