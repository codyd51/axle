#include "write.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/multitasking/pipe.h>
#include <kernel/util/multitasking/std_stream.h>

int std_write(task_t* task, int fd, const void* buf, int len) {
	char* chbuf = (char*)buf;
	int i = 0;
	for (; i < len; i++) {
		putchar(chbuf[i]);
	}
	return i;
}

int write(int fd, char* buf, int len) {
	if (!tasking_installed()) {
		return -1;
	}
	if (!len) return 0;

	//translate address if binary is mapped at an offset in memory
	task_t* current = task_with_pid(getpid());
	if (current->vmem_slide) {
		buf += current->vmem_slide;
	}

	fd_entry ent = current->fd_table[fd];
	if (fd_empty(ent)) {
		//errno = EBADF;
		return -1;
	}

	switch (ent.type) {
		case STD_TYPE:
			return std_write(current, fd, buf, len);
		case FILE_TYPE:
			//TODO implement this!
			//return fwrite(fd, buf, len);
			break;
		case PIPE_TYPE:
		default:
			return pipe_write(fd, buf, len);
			break;
	}
	return -1;
}

