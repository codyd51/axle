#include "syscall.h"
#include "sysfuncs.h"
#include <kernel/interrupts/interrupts.h>
#include <kernel/drivers/terminal/terminal.h>
#include <kernel/drivers/pit/pit.h>
#include <std/array_m.h>

#define MAX_SYSCALLS 128

static int syscall_handler(register_state_t* regs);

array_m* syscalls = {0};

void syscall_init() {
	printf_info("Syscalls init...");

	interrupt_setup_callback(INT_VECTOR_SYSCALL, (int_callback_t)syscall_handler);
	syscalls = array_m_create(MAX_SYSCALLS);
	create_sysfuncs();
}

bool syscall_is_setup() {
	//has the syscalls array been created?
	return (syscalls);
}

void syscall_add(void* syscall) {
	if (syscalls->size + 1 == MAX_SYSCALLS) {
		printf_err("Not installing syscall %d, too many in use!", syscalls->size);
		return;
	}
	array_m_insert(syscalls, syscall);
}

#if defined __i386__
static int syscall_handler(register_state_t* regs) {
	//check requested syscall number
	//stored in eax
	if (!syscalls || regs->eax >= MAX_SYSCALLS) {
		printf_err("Syscall %d called but not defined", regs->eax);
		return -1;
	}

	//location of syscall funcptr
	int (*location)() = (int(*)())array_m_lookup(syscalls, regs->eax);

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
	return ret;
}
#elif defined __x86_64__
static int syscall_handler(register_state_x86_64_t* regs) {
	// Requested syscall number stored in rax
	if (!syscalls || regs->rax >= MAX_SYSCALLS) {
		printf_err("Syscall %d called but not defined", regs->rax);
		return -1;
	}

	// We don't know here how many arguments the syscall handler accepts, so just provide them all.
	uint64_t(*syscall)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = (void(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))array_m_lookup(syscalls, regs->rax);
	// Match the register order that the newlib syscall support passes arguments
	regs->rax = syscall(regs->rbx, regs->rcx, regs->rdx, regs->rsi, regs->rdi);
	return regs->rax;
}
#else 
    FAIL_TO_COMPILE();
#endif
