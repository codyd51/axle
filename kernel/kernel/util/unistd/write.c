#include "write.h"
#include <kernel/util/amc/amc.h>
#include <kernel/multitasking/tasks/task_small.h>

#include <gfx/lib/gfx.h>

int stdout_write(task_small_t* task, int fd, const void* buf, int len) {
	char b[len+16];
	int cnt = snprintf(b, sizeof(b), "[%d] %*", task->id, len, buf);
	printk(b);
	if (b[cnt-1] != '\n') printk("\n");

	// Only forward to the logs viewer if it's active
	if (amc_service_is_active("com.axle.logs_viewer")) {
		amc_message_construct_and_send__from_core("com.axle.logs_viewer", b, cnt);
	}

	return len;
}

int write(int fd, char* buf, int len) {
	assert(tasking_is_active(), "Can't write via fd until multitasking is active");
	if (!len) return 0;

	task_small_t* current = tasking_get_task_with_pid(getpid());
	if (fd != 0) 
	// The old kernel-mode file descriptor mechanism was removed
	assert(fd == 1, "Only FD 1 is supported via this mechanism");
	return stdout_write(current, fd, buf, len);
}
