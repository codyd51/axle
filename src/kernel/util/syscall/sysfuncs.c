#include "sysfuncs.h"
#include <kernel/util/multitasking/tasks/task.h>

void yield() {
	//TODO ensure PIT doesn't fire while we're here
	//go to next task
	task_switch(1);
}

DEFN_SYSCALL1(terminal_writestring, 0, const char*);
DEFN_SYSCALL1(terminal_putchar, 1, char);
DEFN_SYSCALL0(yield, 2);

void create_sysfuncs() {
	syscall_insert(&terminal_writestring);
	syscall_insert(&terminal_putchar);
	syscall_insert(&yield);
}
