#include "sysfuncs.h"
#include <kernel/multitasking/tasks/task_small.h>
#include <std/printf.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/elf/elf.h>
#include <kernel/util/unistd/unistd.h>
#include <user/xserv/api.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/window.h>
#include <gfx/lib/rect.h>
#include <user/xserv/xserv.h>
#include <kernel/util/shmem/shmem.h>
#include <gfx/lib/surface.h>
#include <kernel/util/amc/amc.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/assert.h>

void yield(task_state_t reason) {
	if (!tasking_is_active()) {
		return;
	}

	//if a task is yielding not because it's waiting for i/o, but because it willingly gave up cpu,
	//then it should not be blocked
	//block_task would manage this itself, but we can skip block_task overhead by doing it here ourselves
	if (reason == RUNNABLE) {
		task_switch();
		return;
	}

	panic("not runnable");
	extern task_t* current_task;
	block_task(current_task, reason);
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

char* shmem_create(uint32_t size) {
	task_t* current = task_with_pid(getpid());
	return shmem_get_region_and_map(current->page_dir, size, 0x0, NULL, true);
}

Surface* surface_create(uint32_t width, uint32_t height) {
	return surface_make(width, height, getpid());
}

int aipc_send(char* data, uint32_t size, uint32_t dest_pid, char** destination) {
	return ipc_send(data, size, dest_pid, destination);
}

static void task_assert_wrapper(const char* cmd) {
	task_assert(false, cmd);
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

DEFN_SYSCALL(xserv_win_create, 17, Window*, Rect*);
DEFN_SYSCALL(xserv_win_present, 18, Window*);
DEFN_SYSCALL(xserv_win_destroy, 19, Window*);
DEFN_SYSCALL(xserv_init, 20);

DEFN_SYSCALL(getdents, 21, unsigned int, struct dirent*, unsigned int);
DEFN_SYSCALL(shmem_create, 22, uint32_t);
DEFN_SYSCALL(surface_create, 23, uint32_t, uint32_t);
DEFN_SYSCALL(aipc_send, 24, char*, uint32_t, uint32_t, char**);

// AMC syscalls
DEFN_SYSCALL(amc_register_service, 25, const char*);
DEFN_SYSCALL(amc_message_broadcast, 26, amc_message_t*);
DEFN_SYSCALL(amc_message_await, 27, const char*, amc_message_t**);
DEFN_SYSCALL(amc_message_await_from_services, 28, int, const char**, amc_message_t**);
DEFN_SYSCALL(amc_message_await_any, 29, amc_message_t**);
DEFN_SYSCALL(amc_shared_memory_create, 30, const char*, uint32_t, uint32_t*, uint32_t*);
DEFN_SYSCALL(amc_has_message_from, 31, const char*);
DEFN_SYSCALL(amc_has_message, 32);
DEFN_SYSCALL(amc_launch_service, 33, const char*);
DEFN_SYSCALL(amc_physical_memory_region_create, 34, uint32_t, uint32_t*, uint32_t*);
DEFN_SYSCALL(amc_message_construct_and_send, 35, const char*, uint8_t*, uint32_t);
DEFN_SYSCALL(amc_service_is_active, 36, const char*);

// ADI syscalls
DEFN_SYSCALL(adi_register_driver, 37, const char*, uint32_t);
DEFN_SYSCALL(adi_event_await, 38, uint32_t);
DEFN_SYSCALL(adi_send_eoi, 39, uint32_t);

DEFN_SYSCALL(ms_since_boot, 40);

DEFN_SYSCALL(task_assert_wrapper, 41, const char*);

void create_sysfuncs() {
	syscall_add((void*)&_kill);
	syscall_add((void*)&execve);
	syscall_add((void*)&open);
	syscall_add((void*)&read);
	syscall_add((void*)&output);
	syscall_add((void*)&yield);
	syscall_add((void*)&sbrk);
	syscall_add((void*)&brk);
	syscall_add((void*)&mmap);
	syscall_add((void*)&munmap);
	syscall_add((void*)&lseek);
	syscall_add((void*)&write);
	syscall_add((void*)&exit);
	syscall_add((void*)&sysfork);
	syscall_add((void*)&getpid);
	syscall_add((void*)&waitpid);
	syscall_add((void*)&task_with_pid_auth);
	syscall_add((void*)&xserv_win_create);
	syscall_add((void*)&xserv_win_present);
	syscall_add((void*)&xserv_win_destroy);
	syscall_add((void*)&xserv_init);
	syscall_add((void*)&getdents);
	syscall_add((void*)&shmem_create);
	syscall_add((void*)&surface_create);
	syscall_add((void*)&aipc_send);

	syscall_add((void*)&amc_register_service);
	syscall_add((void*)&amc_message_broadcast);
	syscall_add((void*)&amc_message_await);
	syscall_add((void*)&amc_message_await_from_services);
	syscall_add((void*)&amc_message_await_any);
	syscall_add((void*)&amc_shared_memory_create);
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
