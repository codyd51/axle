#include "task.h"
#include <std/std.h>
#include <std/math.h>
#include <std/memory.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/multitasking/util.h>

#define STACK_MAGIC 0xDEADBEEF
static volatile int next_pid = 1;
volatile task_t* ready_queue;
volatile task_t* current_task;

int getpid() {
	return current_task->id;
}

void yield_int() {
	task_switch(current_task->regs);
}

task_t* create_process(uint32_t eip) {
	task_t* task = kmalloc(sizeof(task_t));
	task->id = next_pid++;

	task->regs.eax = 0;
	task->regs.ebx = 0;
	task->regs.ecx = 0;
	task->regs.edx = 0;
	task->regs.esi = 0;
	task->regs.edi = 0;
	task->regs.eip = (uint32_t)eip;
	task->regs.esp = (uint32_t)kmalloc(0x1000) + 0x1000;

	task->regs.cr3 = ready_queue->regs.cr3;
	task->regs.eflags = ready_queue->regs.eflags;
	
	return task;
}

void add_process(task_t* task) {
	//add to end of ready queue
	task_t* last = ready_queue;
	while (last->next) {
		last = last->next;
	}
	last->next = task;
}

void tasking_install() {
	printf_info("Initializing tasking...");
	
	kernel_begin_critical();
	
	//init first task (kernel task)
	ready_queue = create_process(0);
	//get eflags and cr3
	asm volatile("movl %%cr3, %%eax; movl %%eax, %0;":"=m"(ready_queue->regs.cr3)::"%eax");
	asm volatile("pushfl; movl (%%esp), %%eax; movl %%eax, %0; popfl;":"=m"(ready_queue->regs.eflags)::"%eax");
	current_task = ready_queue;
	
	//create callback to switch tasks
	add_callback((void*)task_switch, 50, 1, 0);

	//reenable interrupts
	kernel_end_critical();

	printf_info("Tasking initialized with kernel PID %d", getpid());
}

volatile void task_switch() {
	if (!ready_queue) {
		return;
	}

	//save current esp, ebp, eip
	uintptr_t esp, ebp, eip;
	asm volatile("mov %%esp, %0" : "=r"(esp));
	asm volatile("mov %%ebp, %0" : "=r"(ebp));
	eip = find_eip();

	//we put a magic value in eip after a task switch so we can catch it here
	if (eip == STACK_MAGIC) {
		//task switch is finished!
		//quit
		return;
	}

	current_task->regs.esp = esp;
	current_task->regs.ebp = ebp;
	current_task->regs.eip = eip;

	//go to next task, or back to the start of the ready queue
	current_task = current_task->next;
	if (!current_task) {
		current_task = ready_queue;
	}

	asm volatile("		\
		mov %0, %%ebx;	\
		mov %1, %%esp;	\
		mov %2, %%ebp;	\
		mov %3, %%cr3;	\
		mov $0xDEADBEEF, %%eax;	\ 
		sti;		\
		jmp *%%ebx 	"
			: : "r"(current_task->regs.eip), "r"(current_task->regs.esp), "r"(current_task->regs.ebp), "r"(current_task->regs.cr3) : "%ebx", "%esp", "%eax");
	//switch_page_directory(current_task->page_directory);
}
