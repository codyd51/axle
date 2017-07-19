#include "write.h"
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/util/multitasking/pipe.h>
#include <kernel/util/multitasking/std_stream.h>

#include <gfx/lib/gfx.h>
#include <gfx/lib/Window.h>
#include <gfx/lib/Label.h>

int xserv_write(task_t* task, int fd, const void* buf, int len) {
	const char* chbuf = (char*)buf;

	Window* xterm = xterm_get();
	Label* output = array_m_lookup(xterm->content_view->labels, 0);

	if (!xterm || !output) {
		return 0;
	}

	printk("xserv_write xterm %x\n", xterm);
	
	int old_len = strlen(output->text);
	int proc_len = strlen(task->name);
	int concat_len = strlen(chbuf);

	char new_str[old_len + len + proc_len];
	memset(new_str, 0, sizeof(new_str));
	strcpy(new_str, output->text);

	//only add prefix if last character of written string is a newline
	if (chbuf[concat_len-1] == '\n') {
		char pid_buf[4] = {0};
		memset(pid_buf, 0, sizeof(pid_buf));
		itoa(task->id, pid_buf);

		strncat(new_str, task->name, proc_len);
		strcat(new_str, "[");
		strncat(new_str, pid_buf, sizeof(pid_buf));
		strncat(new_str, "]: ", 3);
	}

	strncat(new_str, buf, len);

	set_text(output, new_str);

	return len;
}

int std_write(task_t* task, int fd, const void* buf, int len) {
	printk("%s[%d] calls std_write\n", task->name, task->id);
	char* chbuf = (char*)buf;
	int i = 0;

	Window* xterm = xterm_get();
	if (xterm) {
		i = xserv_write(task, fd, buf, len);
	}
	else {
		for (; i < len; i++) {
			putchar(chbuf[i]);
		}
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

