#include "syscall.h"
#include <kernel/util/interrupts/isr.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/util/multitasking/task.h>

static void syscall_handler(registers_t* regs);

void yield() {
	//TODO ensure PIT doesn't fire while we're here
	//go to next task
	task_switch(1);
}

DEFN_SYSCALL1(terminal_writestring, 0, const char*);
DEFN_SYSCALL1(terminal_putchar, 1, char);
DEFN_SYSCALL0(yield, 2);

static void* syscalls[3] = {
	&terminal_writestring,
	&terminal_putchar,
	&yield,
};
uint32_t num_syscalls = 3;

void syscall_install() {
	printf_info("Initializing syscalls...");
	register_interrupt_handler(0x80, &syscall_handler);
}

void syscall_handler(registers_t* regs) {
	//check requested syscall number
	//found in eax
	if (regs->eax >= num_syscalls) return;

	//get required syscall location
	void* location = syscalls[regs->eax];

	//we don't know how many arguments the function wants.
	//so just push them all on the stack in correct order
	//the function will use whatever it wants, and we can just
	//pop it all back off afterwards
	int ret;
	asm volatile("		\
		push %1;	\
		push %2;	\
		push %3;	\
		push %4;	\
		push %5;	\
		call *%6;	\
		pop %%ebx;	\
		pop %%ebx;	\
		pop %%ebx;	\
		pop %%ebx;	\
		pop %%ebx;	\
	" : "=a" (ret) : "r" (regs->edi), "r" (regs->esi), "r" (regs->edx), "r" (regs->ecx), "r" (regs->ebx), "r" (location));
	regs->eax = ret;
}
