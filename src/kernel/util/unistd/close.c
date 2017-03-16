#include "close.h"
#include <kernel/util/multitasking/pipe.h>
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/vfs/fs.h>

int close(int fd) {
	task_t* curr = task_with_pid(getpid());
	fd_entry ent = curr->fd_table[fd];

	if (fd_empty(ent)) {
		return -1;
	}

	int ret = -1;
	switch (ent.type) {
		case STDIN_TYPE:
		case STDOUT_TYPE:
		case STDERR_TYPE:
			ret = 0;
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
}
