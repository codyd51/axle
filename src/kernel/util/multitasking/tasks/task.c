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

#define MAX_TASKS 128

extern page_directory_t* current_directory;
extern page_directory_t* kernel_directory;

static volatile int next_pid = 1;
volatile task_t* current_task;
volatile array_m* tasks;
volatile array_m* blocked;

int getpid() {
	return current_task->id;
}

void block_task(task_t* task, task_state reason) {
	if (!tasking_installed()) return;

	if (task->state != RUNNABLE) {
		return;
	}

	kernel_begin_critical();

	task->state = reason;
	//we assume whatever called this has already updated the task's state
	int idx = array_m_index(tasks, task);
	if (idx >= 0) {
		array_m_remove(tasks, idx);
		if (array_m_index(blocked, task) < 0) {
			array_m_insert(blocked, task);
		}
		else {
			printf_err("%s [%d] was already in blocked", task->name, task->id);
		}
	}
	else {
		printf_err("%s [%d] wasn't in tasks", task->name, task->id);
	}

	kernel_end_critical();
}

void unblock_task(task_t* task) {
	if (!tasking_installed()) return;

	if (task->state == RUNNABLE) {
		return;
	}
	
	kernel_begin_critical();
	task->state = RUNNABLE;
	int idx = array_m_index(blocked, task);
	if (idx >= 0) {
		array_m_remove(blocked, idx);
		if (array_m_index(tasks, task) < 0) {
			array_m_insert(tasks, task);
		}
		else {
			printf_err("%s [%d] was already in tasks", task->name, task->id);
		}
	}
	else {
		printf_err("%s [%d] wasn't in blocked", task->name, task->id);
	}
	kernel_end_critical();
}

task_t* create_process(char* name, uint32_t eip, bool wants_stack) {
	task_t* parent = current_task;

	//clone address space
	page_directory_t* cloned = clone_directory(current_directory);

	//create new process
	task_t* task = (task_t*)kmalloc(sizeof(task_t));
	memset(task, 0, sizeof(task_t));
	task->name = strdup(name);
	task->id = next_pid++;
	task->page_dir = cloned;

	uint32_t current_eip = read_eip();
	if (current_task == parent) {
		task->eip = current_eip;
		return task;
	}
	else {
		return;
	}

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
*/
	
	return task;
}

void add_process(task_t* task) {
	if (!tasking_installed()) return;
	array_m_insert(tasks, task);
}

void idle() {
	while (1) {}
}

bool tasking_installed() {
	return (tasks->size >= 1);
}

void tasking_install() {
	if (tasking_installed()) return;

	printf_info("Initializing tasking...");
	
	kernel_begin_critical();

	move_stack((void*)0xE0000000, 0x2000);

	tasks = array_m_create(MAX_TASKS);
	blocked = array_m_create(MAX_TASKS);

	//init first task (kernel task)
	task_t* kernel = (task_t*)kmalloc(sizeof(task_t));
	memset(kernel, 0, sizeof(task_t));
	kernel->name = "kax";
	kernel->id = next_pid++;
	kernel->page_dir = current_directory;
	current_task = kernel;
	array_m_insert(tasks, kernel);
	
	//create callback to switch tasks
	add_callback((void*)task_switch, 50, true, 0);
	//callback to check up on blocked tasks
	add_callback((void*)update_blocked_tasks, 100, true, 0);

	//idle task
	//runs when anything (including kernel) is blocked for i/o
	if (!fork("idle")) {
		idle();
	}

	//reenable interrupts
	kernel_end_critical();

	printf_info("Tasking initialized with kernel PID %d", getpid());
}

void update_blocked_tasks() {
	if (!tasking_installed()) return;

	for (int i = 0; i < blocked->size; i++) {
		task_t* task = array_m_lookup(blocked, i);
		if (task->state == ZOMBIE) {
			array_m_remove(blocked, i);
		}
		else if (task->state == PIT_WAIT) {
			if (time() >= task->wake_timestamp) {
				unblock_task(task);
			}
		}
		else if (task->state == KB_WAIT) {
			if (haskey()) {
				unblock_task(task);
				goto_pid(task->id);
				return;
			}
		}
	}
}

int fork(char* name) {
	if (!tasking_installed());

	kernel_begin_critical();

	//keep reference to parent for later
	task_t* parent = current_task;

	task_t* child = create_process(name, 0, false);
	add_process(child);

	//THIS LINE will be the entry point for child process
	//(as read_eip will give us the address of this line)
	uint32_t eip = read_eip();

	//eip check above is the entry point when the child starts executing
	//therefore, we could either be the parent or child
	//check!
	if (current_task == parent) {
		//still parent task
		//set up esp/ebp/eip for child
		uint32_t esp, ebp;
		asm volatile("mov %%esp, %0" : "=r"(esp));
		asm volatile("mov %%ebp, %0" : "=r"(ebp));
		child->esp = esp;
		child->ebp = ebp;
		child->eip = eip;

		kernel_end_critical();

		//return child PID by convention
		return child->id;
	}
	else {
		//now executing child process
		//return 0 by convention
		return 0;
	}
}

task_t* next_runnable_task() {
	if (!tasking_installed()) return;

	//find index of currently running task
	int idx = array_m_index(tasks, current_task);
	//if this is the last index loop around to the start of the array
	if (idx + 1 >= tasks->size) {
		//TODO FIX
		//this ASSUMES idle is PID 2
		//(kax is 1)
		/*
		task_t* tmp = array_m_lookup(tasks, 1);
		int diff = strcmp("idle", tmp->name);
		if (diff) {
			proc();
			ASSERT(!diff, "next_runnable_task(): PID %d was not idle, it was %s (strcmp: %d)", 2, tmp->name, diff);
		}
		*/
		/*
		for (int i = 0; i < tasks->size; i++) {
			task_t* tmp = array_m_lookup(tasks, i);
			if (tmp->id == 2) {
				return tmp;
			}
		}
		*/
		return array_m_lookup(tasks, 0);
	}
	//return task at the next index
	return array_m_lookup(tasks, idx + 1);
}

void goto_pid(int id) {
	if (!current_task || !tasks || tasks->size == 0) {
		return;
	}
	kernel_begin_critical();

	//read esp, ebp now for saving later
	uint32_t esp, ebp, eip;
	asm volatile("mov %%esp, %0" : "=r"(esp));
	asm volatile("mov %%ebp, %0" : "=r"(ebp));

	//as in fork(), this returns the address of THIS LINE
	//so when the next process starts executing, it will begin by executing this line
	//to differentiate whether it's the first time it's run and we're trying to actually get EIP or we just started executing the next process, 
	//task_switch() puts a magic value in eax right before switching to the next process
	//that way, we can check if it returned this magic value which indicates that we're executing the next process.
	eip = read_eip();

	//did the next task just start executing?
	if (eip == STACK_MAGIC) {
		return;
	}

	//TODO move this out of task_switch
	//add callback to PIT?

	//haven't switched yet, save old task's values
	current_task->eip = eip;
	current_task->esp = esp;
	current_task->ebp = ebp;

	//switch to PID passed to us
	//TODO optimize this
	//find task with this PID
	for (int i = 0; i < tasks->size; i++) {
		task_t* tmp = array_m_lookup(tasks, i);
		if (tmp->id == id) {
			current_task = tmp;
			break;
		}
	}

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	current_directory = current_task->page_dir;
	task_switch_real(eip, current_directory->physicalAddr, ebp, esp);
}

volatile uint32_t task_switch() {
	//check if idle is only available task
	/*
	if (tasks->size <= 1 || tasks->size >= MAX_TASKS) {
		//don't bother trying to find a task and context switching
		//idle can keep running
		return;
	}
	*/
	//find next runnable task
	task_t* next = next_runnable_task();
	//printf_info("switching to %s [%d]", next->name, next->id);
	goto_pid(next->id);
}

void _kill() {
	if (!tasking_installed()) return;

	kernel_begin_critical();
	block_task(current_task, ZOMBIE);
	update_blocked_tasks();
	task_switch();
	kernel_end_critical();
}

void proc() {
	for (int i = 0; i < tasks->size; i++) {
		task_t* tmp = array_m_lookup(tasks, i);
		printf("%s (PID %d)			|	esp %x	|	ebp %x	|	cr3 %x\n", tmp->name, tmp->id, tmp->esp, tmp->ebp, tmp->page_dir);
	}
}
