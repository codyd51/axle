#include <stdint.h>
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

int exit(uintptr_t code) {
	task_die(code);
	return -1;
}

// TODO(PT): Syscalls should optionally get register state! 
// Then an intentionally called task_assert can print a BT
// Othrewise, regs is null
// How it is now, malformed crashes get valid BTs but well-formed crashes can't get BTs...
static void task_assert_wrapper(register_state_x86_64_t* regs, const char* msg) {
	task_assert(false, msg, regs);
}

void create_sysfuncs() {
	syscall_add((void*)&amc_register_service, false);
	syscall_add((void*)&amc_message_send, false);
	syscall_add((void*)&amc_message_await, false);
	syscall_add((void*)&amc_message_await__u32_event, false);
	syscall_add((void*)&amc_message_await_from_services, false);
	syscall_add((void*)&amc_message_await_any, false);
	syscall_add((void*)&amc_has_message_from, false);
	syscall_add((void*)&amc_has_message, false);

	syscall_add((void*)&adi_register_driver, false);
	syscall_add((void*)&adi_event_await, false);
	syscall_add((void*)&adi_send_eoi, false);

	syscall_add((void*)&sbrk, false);
	syscall_add((void*)&write, false);
	syscall_add((void*)&exit, false);
	syscall_add((void*)&getpid, false);
	syscall_add((void*)&ms_since_boot, false);
	// task_assert() needs a register snapshot to construct backtraces
	syscall_add((void*)&task_assert_wrapper, true);
}
