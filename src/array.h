#ifndef ARRAY_H_
#define ARRAY_H_

struct array;

struct array *array_create(long slots, long stride);
void array_destroy(struct array *array);

void *array_new(struct array *array);
void array_del(struct array *array, void *ptr);
void *array_get(struct array *array, long idx);

void array_init_lock(struct array *array);
void *array_locked_new(struct array *array);
void array_locked_del(struct array *array, void *ptr);
void *array_locked_get(struct array *array, long idx);

void array_report(struct array *array);

#endif //ARRAY_H_
