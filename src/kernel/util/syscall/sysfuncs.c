#include "sysfuncs.h"

extern task_t* current_task;

void yield(task_state reason) {
	kernel_begin_critical();
	block_task(current_task, reason);
	kernel_end_critical();

	//go to next task
	task_switch();
}

DEFN_SYSCALL1(terminal_writestring, 0, const char*);
DEFN_SYSCALL1(terminal_putchar, 1, char);
DEFN_SYSCALL1(yield, 2, task_state);

void create_sysfuncs() {
	sys_insert((void*)&terminal_writestring);
	sys_insert((void*)&terminal_putchar);
	sys_insert((void*)&yield);
}
