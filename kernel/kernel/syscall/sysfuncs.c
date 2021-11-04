#include "sysfuncs.h"
#include <kernel/multitasking/tasks/task_small.h>
#include <std/printf.h>
#include <kernel/util/elf/elf.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/rect.h>
#include <kernel/util/amc/amc.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/assert.h>
#include <kernel/util/unistd/write.h>

void* sbrk(int increment);

int exit(int code) {
	task_die(code);
	return -1;
}

static void task_assert_wrapper(const char* cmd) {
	task_assert(false, cmd, NULL);
}

void create_sysfuncs() {
	syscall_add((void*)&amc_register_service);
	syscall_add((void*)&amc_message_send);
	syscall_add((void*)&amc_message_await);
	syscall_add((void*)&amc_message_await_from_services);
	syscall_add((void*)&amc_message_await_any);
	syscall_add((void*)&amc_has_message_from);
	syscall_add((void*)&amc_has_message);

	syscall_add((void*)&adi_register_driver);
	syscall_add((void*)&adi_event_await);
	syscall_add((void*)&adi_send_eoi);

	syscall_add((void*)&sbrk);
	syscall_add((void*)&write);
	syscall_add((void*)&exit);
	syscall_add((void*)&getpid);
	syscall_add((void*)&ms_since_boot);
	syscall_add((void*)&task_assert_wrapper);
}
