#ifndef WORK_GROUP_H_
#define WORK_GROUP_H_

struct work_group;
struct work_group *work_group_create();
void work_group_release(struct work_group *grp);
void work_group_add(struct work_group *grp, void (*fp)(void*), void *arg);
void work_group_dispatch(struct work_group *grp, void (*fp)(void*), void *arg);
void work_group_dispatch_array(struct work_group *grp);

#endif //WORK_GROUP_H_
