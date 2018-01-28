#include "fd.h"
#include <kernel/util/multitasking/tasks/task.h>
#include <stdbool.h>

bool fd_empty(fd_entry entry) {
	bool empty = (entry.payload == 0);
	return empty;
}

void fd_remove(task_t* task, int index) {
	//should this function do the work of cleaning up resources, or just remove the file descriptor from the table?
	//for now, just removes entry from table
	//a null payload marks an empty index
	task->fd_table[index].payload = NULL;
}

int fd_add(task_t* task, fd_entry entry) {
	//go through task's file descriptor table, looking for an empty slot
	for (int i = 0; i < FD_MAX; i++) {
		if (fd_empty(task->fd_table[i])) {
			//found an empty slot!
			task->fd_table[i] = entry;
			return i;
		}
	}
	ASSERT(0, "PID %d ran out of file descriptors!", task->id);
	return -1;
}

int fd_add_index(task_t* task, fd_entry entry, int index) {
	fd_remove(task, index);
	task->fd_table[index] = entry;
	return index;
}

