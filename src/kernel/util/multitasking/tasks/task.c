#include "task.h"
#include <std/std.h>
#include <std/math.h>
#include <std/memory.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/multitasking/util.h>
#include <kernel/util/paging/paging.h>

//magic value placed in eax at end of task switch
//we read eax when trying to catch current eip
//if this value is in eax, we know we already caught eip and that the task switch is over, so it should return immediately
#define STACK_MAGIC 0xDEADBEEF

static volatile int next_pid = 1;
volatile task_t* ready_queue;
volatile task_t* current_task;
extern page_directory_t* current_directory;
extern page_directory_t* kernel_directory;

int getpid() {
	return current_task->id;
}

task_t* create_process(uint32_t eip) {
	task_t* task = kmalloc(sizeof(task_t));
	memset(task, 0, sizeof(task_t));
	task->id = next_pid++;

	task->state = RUNNABLE;
	task->wake_timestamp = 0;

	task->esp = (uint32_t)kmalloc_a(0x1000) + 0x1000;
	uint32_t* stack = task->esp;
	
	//cpu data (iret)
	*--stack = 0x202; //eflags
	*--stack = 0x8; //cs
	*--stack = (uint32_t)eip; //eip

	//irq_common_stub_ret adds 8 to esp to skip isr number and error code
	//compensate for that offset here
	*--stack = 0x0;
	*--stack = 0x0;

	//emulates pusha
	*--stack = 0xAAAAAAAA; //eax
	*--stack = 0xCCCCCCCC; //ecx
	*--stack = 0xDDDDDDDD; //edx
	*--stack = 0xBBBBBBBB; //ebx

	*--stack = 0x0; //esp (ignored)
	*--stack = 0x0; //ebp
	*--stack = 0x0; //esi
	*--stack = 0x0; //edi
	
	//data segments
	*--stack = 0x10; //ds
	*--stack = 0x10; //es
	*--stack = 0x10; //fs
	*--stack = 0x10; //gs

	task->esp = stack;
	//task->esp += 0x1000;

	task->switched_once = false;

	printf_dbg("made task with esp %x, eip %x", task->esp, eip);
	
	return task;
}

void add_process(task_t* task) {
	//add to end of ready queue
	task_t* last = current_task;
	while (last->next) {
		last = last->next;
	}
	last->next = task;
}

void idle() {
	//spin
	printf_info("idle ran");
	char test[128];
	memset(test, '7', 127);
	while (1) {
		//strcpy(&test, "This is a test of stack data!");
	}
}

void test() {
	printf_info("test thread start");
	while (1) {
	}
}

void tasking_install() {
	printf_info("Initializing tasking...");
	
	kernel_begin_critical();

	move_stack((void*)0xE0000000, 0x2000);
	
	//init first task (kernel task)
	ready_queue = create_process(idle);
	current_task = ready_queue;

	//init idle task
	//runs when anything (including kernel) is blocked for i/o
	//task_t* idle = create_process(idle);
	//add_process(idle);
	
	//create callback to switch tasks
	//add_callback((void*)task_switch, 10, true, 0);

	//reenable interrupts
	kernel_end_critical();

	printf_info("Tasking initialized with kernel PID %d", getpid());
}

volatile void task_switch(uint32_t esp) {
	if (!ready_queue) {
		return;
	}

	printf_dbg("Task switch stack dump: ");
	dump_stack(esp);

	//save current esp
	//uint32_t esp;
	//asm volatile("mov %%esp, %0" : "=r"(esp));
	
	if (!current_task->switched_once) {
		printf_info("PID %d preempted for the first time, using esp %x", current_task->id, current_task->esp);
		esp = current_task->esp;
		current_task->switched_once = true;
	}

	//did not switch tasks yet
	//save register values and switch
	current_task->esp = esp;

	int old_id = current_task->id;
	//go to next task, or back to the start of the ready queue
	current_task = current_task->next;
	if (!current_task) {
		current_task = ready_queue;
	}

	esp = current_task->esp;

//	set_kernel_stack(current_task->kernel_stack + 0x1000);

	perform_task_switch(esp);
}
