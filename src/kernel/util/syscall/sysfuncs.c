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

DEFN_SYSCALL0(kill,		0);
DEFN_SYSCALL3(execve,	1, char*		, char**, char**);
DEFN_SYSCALL2(open,		2, const char*	, int);
DEFN_SYSCALL3(read,		3, int			, char*	, size_t);
DEFN_SYSCALL2(output,	4, int			, char*);
DEFN_SYSCALL1(yield,	5, task_state);
DEFN_SYSCALL1(sbrk,		6, int);
DEFN_SYSCALL1(brk,		7, void*);
DEFN_SYSCALL5(mmap,		8, void*		, int,	  int,		int,	int);
DEFN_SYSCALL2(munmap,	9, void*		, int);
DEFN_SYSCALL3(lseek,   10, int			, int,	  int);
DEFN_SYSCALL3(write,   11, int			, char*,	  int);
DEFN_SYSCALL1(_exit,   12, int);
DEFN_SYSCALL0(fork,	   13);
DEFN_SYSCALL0(getpid,  14);
DEFN_SYSCALL3(waitpid, 15,	 int		, int*,	  int);
DEFN_SYSCALL1(task_with_pid, 16,	 int);

DEFN_SYSCALL2(xserv_win_create, 17,	 Window*, Rect*);
DEFN_SYSCALL1(xserv_win_present, 18, Window*);
DEFN_SYSCALL1(xserv_win_destroy, 19, Window*);
DEFN_SYSCALL0(xserv_init, 20);

DEFN_SYSCALL3(getdents, 21, unsigned int, struct dirent*, unsigned int);

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
}

