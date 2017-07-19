#ifndef WRITE_H
#define WRITE_H

#include <kernel/util/multitasking/tasks/task.h>

int write(int fd, char* buf, int len);
int std_write(task_t* task, int fd, const void* buf, int len);

#endif
