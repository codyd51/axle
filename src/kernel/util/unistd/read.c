#include "read.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/vfs/fs.h>

int stdin_read(task_t* task, void* buf, uint32_t count) {
	char* chbuf = (char*)buf;
	//if tasking isn't installed, we can't do anything
	if (!tasking_installed() || !task || !task->stdin_buf) {
		return 0;
	}

	//if we have no characters to read,
	//block until we do
	if (!task->stdin_buf->count) {
		//sys_yield(task, KB_WAIT);
		//block_task(task, KB_WAIT);
		//return stdin_read(task, buf, count);
		printf("stdin_read blocking task %d, current = %d\n", task->id, getpid());
	}
	//we should now have characters to read

	int i = 0;
	for (; i < count && task->stdin_buf->count > 0; i++) {
		//TODO this should block if cb has no data available!
		//printf("popping from stdin buf ");
		cb_pop_front(task->stdin_buf, &(chbuf[i]));
		//printf("(%c)\n", chbuf[i]);
		//quit early on newline or EOF
		//TODO this should only happen when terminal is cooked
		if (chbuf[i] == '\n' || chbuf[i] == EOF) {
			break;
		}
	}
	chbuf[i+1] = '\0';
	return i+1;
}

int stdout_read(task_t* task, void* buf, uint32_t count) {
	char* chbuf = (char*)buf;
	//if tasking isn't installed, we can't do anything
	if (!tasking_installed() || !task || !task->stdout_buf) {
		return 0;
	}

	int i = 0;
	for (; i < count && task->stdout_buf->count > 0; i++) {
		//TODO this should block if cb has no data available!
		cb_pop_front(task->stdout_buf, &(chbuf[i]));
	}
	chbuf[i+1] = '\0';
	return i+1;
}

int stderr_read(task_t* task, void* buf, uint32_t count) {
	char* chbuf = (char*)buf;
	//if tasking isn't installed, we can't do anything
	if (!tasking_installed() || !task || !task->stderr_buf) {
		return 0;
	}

	int i = 0;
	for (; i < count && task->stderr_buf->count > 0; i++) {
		//TODO this should block if cb has no data available!
		cb_pop_front(task->stderr_buf, &(chbuf[i]));
	}
	chbuf[i+1] = '\0';
	return i+1;
}

uint32_t read_proc(task_t* task, int fd, void* buf, uint32_t count) {
	unsigned char* chbuf = buf;
	memset(chbuf, 0, count);

	//find fd_entry corresponding to this fd
	fd_entry ent = task->fd_table[fd];
	if (fd_empty(ent)) {
		//errno = EBADF;
		return -1;
	}

	if (fd == 0) {
		printf("read_proc() fd 0 type = %d\n", ent.type);
	}

	//what type of file descriptor is this?
	//dispatch appropriately
	switch (ent.type) {
		case STDIN_TYPE:
			return stdin_read(task, buf, count);
			break;
		case STDOUT_TYPE:
			return stdout_read(task, buf, count);
			break;
		case STDERR_TYPE:
			return stderr_read(task, buf, count);
			break;
		case FILE_TYPE:
			return fread(buf, sizeof(char), count, (FILE*)ent.payload);
		case PIPE_TYPE:
		default:
			return pipe_read(fd, buf, count);
	}
	return -1;
}

uint32_t read(int fd, void* buf, uint32_t count) {
	task_t* current = task_with_pid(getpid());
	return read_proc(current, fd, buf, count);
}

