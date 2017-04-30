#include "read.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/util/multitasking/std_stream.h>

#include <gfx/lib/gfx.h>
#include <gfx/lib/Window.h>
#include <gfx/lib/Label.h>

int std_read(task_t* task, int fd, void* buf, int count) {
	char* chbuf = (char*)buf;
	int i = 0;
	//TODO implement newline_wait
	//

	for (; i < count - 1; i++) {
		char ch = std_stream_popc(task);
		chbuf[i] = ch;
		if (ch == -1) {
			//no more items to read!
			break;
		}

		if (ch == '\n' || ch == '\0') {
			break;
		}
	}
	//Window* xterm = xterm_get();
	//std_write(task, fd, buf, i+1);
	return i+1;
}

uint32_t read(int fd, void* buf, uint32_t count) {
	if (!tasking_installed()) {
		return -1;
	}
	if (!count) {
		return 0;
	}

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
		case STD_TYPE:
			return std_read(current, fd, buf, count);
		case FILE_TYPE:
			return fread(buf, sizeof(char), count, (FILE*)ent.payload);
		case PIPE_TYPE:
		default:
			return pipe_read(fd, buf, count);
	}
	return -1;
}

