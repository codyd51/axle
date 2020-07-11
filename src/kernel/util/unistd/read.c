#include "read.h"
#include <kernel/multitasking/fd.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/multitasking/pipe.h>

#include <gfx/lib/gfx.h>
#include <gfx/lib/Window.h>
#include <gfx/lib/Label.h>

int stdin_read(task_small_t* task, int UNUSED(fd), void* buf, int count) {
	char* chbuf = (char*)buf;
	int i = 0;
	for (; i < count; i++) {
		//printf("Reading from stream: %d\n", task->stdin_stream->buf->count);
		char ch = std_stream_popchar(task->stdin_stream);
		chbuf[i] = ch;
	}
	//Window* xterm = xterm_get();
	//std_write(task, fd, buf, i+1);
	return i;
}

uint32_t read(int fd, void* buf, uint32_t count) {
	assert(tasking_is_active(), "Can't read via fd until multitasking is active");
	if (!count) return 0;

	task_small_t* current = tasking_get_task_with_pid(getpid());
	// Find the stream associated with the file descriptor
	fd_entry_t* fd_ent = array_l_lookup(current->fd_table, fd);

	if (fd_ent->type == STD_TYPE) {
		return stdin_read(current, fd, buf, count);
	}

	NotImplemented();
	/*
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
	*/
}
