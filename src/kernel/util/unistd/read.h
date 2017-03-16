#ifndef READ_H
#define READ_H

#include <stdint.h>
#include <std/std.h>
#include <kernel/util/multitasking/tasks/task.h>

uint32_t read(int fd, void* buf, uint32_t count);
uint32_t read_proc(task_t* task, int fd, void* buf, uint32_t count);

#endif
