#ifndef TASK_SMALL_INTERNAL_H
#define TASK_SMALL_INTERNAL_H

#define MAX_TASKS 1024

task_small_t* _tasking_get_linked_list_head(void);
void _tasking_set_linked_list_head(task_small_t* new_head);

#endif