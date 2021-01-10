#include "write.h"
#include <kernel/multitasking/fd.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/multitasking/pipe.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/util/amc/amc.h>
#include <user/xserv/xserv.h>

#include <gfx/lib/gfx.h>
#include <gfx/lib/Window.h>
#include <gfx/lib/Label.h>

int xserv_write(task_t* task, int UNUSED(fd), const void* buf, int len) {
	Deprecated();
	return -1;
	/*
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
	*/
}

int stdout_write(task_small_t* task, int fd, const void* buf, int len) {
	// Write to the standard out buffer, then immediately flush the buffer
	// These steps can be separated when necessary
	/*
	std_stream_push(task->stdout_stream, buf, len);
	uint32_t char_count_remaining = len;
	while (char_count_remaining > 0) {
		char buf[64];
		memset(buf, 0, sizeof(buf));
		printk("Requsting %d\n", MIN(sizeof(buf), char_count_remaining));
		uint32_t char_count_processed_now = std_stream_pop(task->stdoueet_stream, &buf, MIN(sizeof(buf), char_count_remaining));
		buf[char_count_processed_now] = '\0';
		printk("Processed now: %d, %s\n", char_count_processed_now, buf);
		printk("next iter\n");
		char_count_remaining -= char_count_processed_now;
		//printf(buf);
	}
	*/

	// This string is mapped in the process performing the syscall,
	// but copy it to kernel memory so we can forward it to any other process
	// Also, the buffer may not be null-terminated at `len`, so we need to null terminate it ourselves
	char copy_buf[len+1];
	strncpy(&copy_buf, buf, len);
	copy_buf[len] = '\0';

	printk("[PID %d] %s", task->id, copy_buf);
	if (copy_buf[len-1] != '\n') printk("\n");

		char* chbuf = (char*)&copy_buf;
		int bytes_remaining = len;
		int stride = 40;
		for (int i = 0; i < len; i += stride) {
			stride = min(stride, bytes_remaining);
			bytes_remaining -= stride;

			amc_message_t* amc_msg = amc_message_construct__from_core(chbuf + i, stride);
			amc_message_send("com.axle.tty", amc_msg);
	}

	return len;
}

int write(int fd, char* buf, int len) {
	assert(tasking_is_active(), "Can't write via fd until multitasking is active");
	if (!len) return 0;

	task_small_t* current = tasking_get_task_with_pid(getpid());
	// Find the stream associated with the file descriptor
	fd_entry_t* fd_ent = array_l_lookup(current->fd_table, fd);

	if (fd_ent->type == STD_TYPE) {
		return stdout_write(current, fd, buf, len);
	}

	NotImplemented();
	return -1;

	/*
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
	*/
}
