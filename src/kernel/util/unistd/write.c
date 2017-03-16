#include "write.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/pipe.h>
#include <std/std.h>
#include <kernel/util/vfs/fs.h>

int stdin_write(task_t* task, const void* buf, int len) {
	char* chbuf = (char*)buf;
	//if tasking isn't available, 
	//then writing to stdin has no meaning
	//in that case, quit early
	if (!tasking_installed() || !task || !task->stdin_buf) {
		return -1;
	}

	//add to stdin buffer
	int i = 0;
	for (; i < len; i++) {
		cb_push_back(task->stdin_buf, &(chbuf[i]));
	}
	update_blocked_tasks();
	return i;
}

int stdout_write(task_t* task, const void* buf, int len) {
	char* chbuf = (char*)buf;
	//if tasking isn't available, just output
	if (!tasking_installed() || !task || !task->stdout_buf) {
		for (int i = 0; i < len; i++) {
			outputc(TERM_OUTPUT, chbuf[i]);
		}
		return;
	}

	//add to stdout buffer
	for (int i = 0; i < len; i++) {
		cb_push_back(task->stdout_buf, &(chbuf[i]));
		
		//flush on newline and EOF
		//also flush if buffer is full
		if (chbuf[i] == '\n' || chbuf[i] == EOF || task->stdout_buf->count >= task->stdout_buf->capacity) {
			for (int i = 0; i < task->stdout_buf->count; i++) {
				char ch;
				cb_pop_front(task->stdout_buf, &ch);
				outputc(TERM_OUTPUT, ch);
			}
		}
	}
}

int stderr_write(task_t* task, const void* buf, int len) {
	char* chbuf = (char*)buf;
	//if tasking isn't available, just output
	if (!tasking_installed() || !task || !task->stderr_buf) {
		for (int i = 0; i < len; i++) {
			outputc(TERM_OUTPUT, chbuf[i]);
		}
		return;
	}

	//add to stderr buffer
	for (int i = 0; i < len; i++) {
		//stderr is not buffered,
		//we can directly output any message we get
		//thus, don't bother adding to circular buffer,
		//just output directly
		outputc(TERM_OUTPUT, chbuf[i]);
		/*
		cb_push_back(task->stderr_buf, &(chbuf[i]));
		
		//flush on newline and EOF
		if (chbuf[i] == '\n' || chbuf[i] == EOF) {
			for (int i = 0; i < task->stderr_buf->count; i++) {
				char ch;
				cb_pop_front(task->stderr_buf, &ch);
				outputc(TERM_OUTPUT, ch);
			}
		}
		*/
	}
}

int write_proc(task_t* task, int fd, const void* buf, int len) {
	if (!tasking_installed()) {
		if (fd == stdout) {
			return stdout_write(NULL, buf, len);
		}
	}

	fd_entry ent = task->fd_table[fd];
	if (fd_empty(ent)) {
		//errno = EBADF;
		return -1;
	}

	switch (ent.type) {
		case STDIN_TYPE:
			return stdin_write(task, buf, len);
			break;
		case STDOUT_TYPE:
			return stdout_write(task, buf, len);
			break;
		case STDERR_TYPE:
			return stderr_write(task, buf, len);
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

int write(int fd, const void* buf, int len) {
	task_t* current = task_with_pid(getpid());
	return write_proc(current, fd, buf, len);
}

