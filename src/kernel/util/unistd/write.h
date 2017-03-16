#ifndef WRITE_H
#define WRITE_H

#include <kernel/util/multitasking/tasks/task.h>

int write(int fd, const void* buf, int len);
int write_proc(task_t* task, int fd, const void* buf, int len);

#endif
