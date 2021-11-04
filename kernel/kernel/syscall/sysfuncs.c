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

void _null(void) {}

void yield(task_state_t reason) {
	Deprecated();
}

int lseek(int UNUSED(fd), int UNUSED(offset), int UNUSED(whence)) {
	printf("lseek called\n");
	return 0;
}

int exit(int code) {
	task_die(code);
	return -1;
	//printf("[%s [%d]] EXIT status code %d\n", current->name, current->id, code);
}

int sysfork() {
	Deprecated();
	return -1;
	//return fork(current_task->name);
}

static void task_assert_wrapper(const char* cmd) {
	task_assert(false, cmd, NULL);
}

DEFN_SYSCALL(kill, 0);
DEFN_SYSCALL(execve, 1, char*, char**, char**);
DEFN_SYSCALL(open, 2, const char*, int);
DEFN_SYSCALL(read, 3, int, char*, size_t);
DEFN_SYSCALL(output, 4, int, char*);
DEFN_SYSCALL(yield, 5, task_state_t);
DEFN_SYSCALL(sbrk, 6, int);
DEFN_SYSCALL(brk, 7, void*);
DEFN_SYSCALL(mmap, 8, void*, int, int, int, int);
DEFN_SYSCALL(munmap, 9, void*, int);
DEFN_SYSCALL(lseek, 10, int, int, int);
DEFN_SYSCALL(write, 11, int, char*, int);
DEFN_SYSCALL(_exit, 12, int);
DEFN_SYSCALL(fork, 13);
DEFN_SYSCALL(getpid, 14);
DEFN_SYSCALL(waitpid, 15, int, int*, int);
DEFN_SYSCALL(task_with_pid, 16, int);

// AMC syscalls
DEFN_SYSCALL(amc_register_service, 17, const char*);
DEFN_SYSCALL(amc_message_broadcast, 18, amc_message_t*);
DEFN_SYSCALL(amc_message_await, 19, const char*, amc_message_t**);
DEFN_SYSCALL(amc_message_await_from_services, 20, int, const char**, amc_message_t**);
DEFN_SYSCALL(amc_message_await_any, 21, amc_message_t**);
DEFN_SYSCALL(amc_has_message_from, 22, const char*);
DEFN_SYSCALL(amc_has_message, 23);
DEFN_SYSCALL(amc_launch_service, 24, const char*);
DEFN_SYSCALL(amc_physical_memory_region_create, 25, uint32_t, uintptr_t*, uintptr_t*);
DEFN_SYSCALL(amc_message_construct_and_send, 26, const char*, uint8_t*, uint32_t);
DEFN_SYSCALL(amc_service_is_active, 27, const char*);

// ADI syscalls
DEFN_SYSCALL(adi_register_driver, 28, const char*, uint32_t);
DEFN_SYSCALL(adi_event_await, 29, uint32_t);
DEFN_SYSCALL(adi_send_eoi, 30, uint32_t);

DEFN_SYSCALL(ms_since_boot, 31);

DEFN_SYSCALL(task_assert_wrapper, 32, const char*);

void* sbrk(int increment);

void create_sysfuncs() {
	syscall_add((void*)&_null);
	syscall_add((void*)&_null);
	syscall_add((void*)&open);
	syscall_add((void*)&read);
	syscall_add((void*)&output);
	syscall_add((void*)&yield);
	syscall_add((void*)&sbrk);
	syscall_add((void*)&_null);
	syscall_add((void*)&_null);
	syscall_add((void*)&_null);
	syscall_add((void*)&lseek);
	syscall_add((void*)&write);
	syscall_add((void*)&exit);
	syscall_add((void*)&sysfork);
	syscall_add((void*)&getpid);
	syscall_add((void*)&_null);
	syscall_add((void*)&_null);

	syscall_add((void*)&amc_register_service);
	syscall_add((void*)&amc_message_broadcast);
	syscall_add((void*)&amc_message_await);
	syscall_add((void*)&amc_message_await_from_services);
	syscall_add((void*)&amc_message_await_any);

	syscall_add((void*)&amc_has_message_from);
	syscall_add((void*)&amc_has_message);
	syscall_add((void*)&amc_launch_service);
	syscall_add((void*)&amc_physical_memory_region_create);
	syscall_add((void*)&amc_message_construct_and_send);
	syscall_add((void*)&amc_service_is_active);

	syscall_add((void*)&adi_register_driver);
	syscall_add((void*)&adi_event_await);
	syscall_add((void*)&adi_send_eoi);

	syscall_add((void*)&ms_since_boot);
	syscall_add((void*)&task_assert_wrapper);
}
