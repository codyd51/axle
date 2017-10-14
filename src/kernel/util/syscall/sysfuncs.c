#include "sysfuncs.h"
#include <kernel/util/multitasking/tasks/task.h>
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

void yield(task_state reason) {
	if (!tasking_installed()) return;

	//if a task is yielding not because it's waiting for i/o, but because it willingly gave up cpu,
	//then it should not be blocked
	//block_task would manage this itself, but we can skip block_task overhead by doing it here ourselves
	if (reason == RUNNABLE) {
		task_switch(true);
		return;
	}
	extern task_t* current_task;
	block_task(current_task, reason);
}

int lseek(int UNUSED(fd), int UNUSED(offset), int UNUSED(whence)) {
	printf("lseek called\n");
	return 0;
}

extern task_t* current_task;
int exit(int code) {
	current_task->exit_code = code;
	_kill();
	return code;
}

int sysfork() {
	return fork(current_task->name);
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

DEFN_SYSCALL(kill, 0);
DEFN_SYSCALL(execve, 1, char*, char**, char**);
DEFN_SYSCALL(open, 2, const char*, int);
DEFN_SYSCALL(read, 3, int, char*, size_t);
DEFN_SYSCALL(output, 4, int, char*);
DEFN_SYSCALL(yield, 5, task_state);
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

void create_sysfuncs() {
	sys_insert((void*)&_kill);
	sys_insert((void*)&execve);
	sys_insert((void*)&open);
	sys_insert((void*)&read);
	sys_insert((void*)&output);
	sys_insert((void*)&yield);
	sys_insert((void*)&sbrk);
	sys_insert((void*)&brk);
	sys_insert((void*)&mmap);
	sys_insert((void*)&munmap);
	sys_insert((void*)&lseek);
	sys_insert((void*)&write);
	sys_insert((void*)&exit);
	sys_insert((void*)&sysfork);
	sys_insert((void*)&getpid);
	sys_insert((void*)&waitpid);
	sys_insert((void*)&task_with_pid_auth);
	sys_insert((void*)&xserv_win_create);
	sys_insert((void*)&xserv_win_present);
	sys_insert((void*)&xserv_win_destroy);
	sys_insert((void*)&xserv_init);
	sys_insert((void*)&getdents);
	sys_insert((void*)&shmem_create);
	sys_insert((void*)&surface_create);
	sys_insert((void*)&aipc_send);
}
