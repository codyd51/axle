#include "dup.h"
#include <kernel/multitasking/tasks/task.h>
#include <kernel/multitasking/fd.h>
#include <kernel/util/unistd/close.h>

int dup(int fd) {
	NotImplemented();
	/*
	task_t* current = task_with_pid(getpid());
	fd_entry ent = current->fd_table[fd];

	//ensure fd is a valid descriptor
	if (fd_empty(ent)) {
		return -1;
	}

	fd_entry new_ent;
	new_ent.type = ent.type;
	new_ent.payload = ent.payload;
	return fd_add(current, new_ent);
	*/
}

int dup2(int fd, int newfd) {
	NotImplemented();
	/*
	task_t* current = task_with_pid(getpid());
	fd_entry ent = current->fd_table[fd];

	//ensure fd is a valid descriptor
	if (fd_empty(ent)) {
		return -1;
	}

	//close() newfd to ensure we can use it
	//if newfd was in use, close() will clean up resources
	//if it wasn't, close() will silently fail (which is fine)
	close(newfd);

	//now, copy entry at fd into newfd
	return fd_add_index(current, ent, newfd);
	*/
}
