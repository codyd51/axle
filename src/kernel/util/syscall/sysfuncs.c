#include "sysfuncs.h"

void yield(task_state reason) {
	if (!tasking_installed()) return;

	//if a task is yielding not because it's waiting for i/o, but because it willingly gave up cpu,
	//then it should not be blocked
	//block_task would manage this itself, but we can skip block_task overhead by doing it here ourselves
	if (reason == RUNNABLE) {
		task_switch();
		return;
	}
	extern task_t* current_task;
	block_task(current_task, reason);
}

DEFN_SYSCALL1(terminal_writestring, 0, const char*);
DEFN_SYSCALL1(terminal_putchar, 1, char);
DEFN_SYSCALL1(yield, 2, task_state);
DEFN_SYSCALL3(read, 3, int, void*, size_t);

void create_sysfuncs() {
	sys_insert((void*)&terminal_writestring);
	sys_insert((void*)&terminal_putchar);
	sys_insert((void*)&yield);
	// sys_insert((void*)&read);
}
