#include "close.h"
#include <kernel/multitasking/pipe.h>
#include <kernel/multitasking/fd.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/util/vfs/fs.h>

int close(int fd) {
	NotImplemented();
	/*
	task_t* curr = task_with_pid(getpid());
	fd_entry ent = curr->fd_table[fd];

	if (fd_empty(ent)) {
		return -1;
	}

	int ret = -1;
	switch (ent.type) {
		case STD_TYPE:
			ret = -1;
			break;
		case FILE_TYPE:
			fclose((FILE*)ent.payload);
			ret = 0;
			break;
		case PIPE_TYPE:
		default:
			ret = pipe_close(fd);
			break;
	}

	fd_remove(curr, fd);
	return ret;
	*/
}
