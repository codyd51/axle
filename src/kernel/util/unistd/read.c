#include "read.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/vfs/fs.h>

int stdin_read(void* buf, uint32_t count) {
	char* chbuf = (char*)buf;
	int i = 0;
	for (; i < count; i++) {
		chbuf[i] = getchar();
		putchar(chbuf[i]);
		//quit early on newline
		//TODO this should only happen when terminal is cooked
		if (chbuf[i] == '\n') {
			break;
		}
	}
	chbuf[i+1] = '\0';
	return i+1;
}

int stdout_read(void* buf, uint32_t count) {
	char* chbuf = (char*)buf;
	char* tst = "stdout_read test message";
	int i = 0;
	for (; i < count; i++) {
		chbuf[i] = tst[i];
	}
	chbuf[i+1] = '\0';
	return i;
}

uint32_t read(int fd, void* buf, uint32_t count) {
	unsigned char* chbuf = buf;
	memset(chbuf, 0, count);

	//find fd_entry corresponding to this fd
	task_t* current = task_with_pid(getpid());
	fd_entry ent = current->fd_table[fd];
	if (fd_empty(ent)) {
		//errno = EBADF;
		return -1;
	}

	//what type of file descriptor is this?
	//dispatch appropriately
	switch (ent.type) {
		case STDIN_TYPE:
			return stdin_read(buf, count);
			break;
		case STDOUT_TYPE:
		case STDERR_TYPE:
			return stdout_read(buf, count);
			break;
		case FILE_TYPE:
			return fread(buf, sizeof(char), count, (FILE*)ent.payload);
		case PIPE_TYPE:
		default:
			return pipe_read(fd, buf, count);
	}
	return -1;
}

