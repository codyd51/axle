#include "write.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/multitasking/pipe.h>

int stdin_write(const void* buf, int len) {

}

int stdout_write(const void* buf, int len) {
	char* chbuf = buf;
	int i = 0;
	for (; i < len && chbuf[i+1] != '\0'; i++) {
		putchar(chbuf[i]);
	}
	return i;
}

int stderr_write(const void* buf, int len) {

}

int write(int fd, const void* buf, int len) {
	task_t* current = task_with_pid(getpid());
	fd_entry ent = current->fd_table[fd];
	if (fd_empty(ent)) {
		//errno = EBADF;
		return -1;
	}

	switch (ent.type) {
		case STDIN_TYPE:
			return stdin_write(buf, len);
			break;
		case STDOUT_TYPE:
			return stdout_write(buf, len);
			break;
		case STDERR_TYPE:
			return stderr_write(buf, len);
			break;
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

