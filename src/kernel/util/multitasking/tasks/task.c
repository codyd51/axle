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
#define MAX_FILES 32

extern page_directory_t* current_directory;
extern page_directory_t* kernel_directory;

static volatile int next_pid = 1;
volatile task_t* current_task;
volatile array_m* tasks;
volatile array_m* blocked;

void stdin_read(char* buf, uint32_t count);
void stdout_read(char* buffer, uint32_t count);
void stderr_read(char* buffer, uint32_t count);
static void setup_fds(task_t* task) {
	task->files = array_m_create(MAX_FILES);
	array_m_insert(task->files, stdin_read);
	array_m_insert(task->files, stdout_read);
	array_m_insert(task->files, stderr_read);
}


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
	
	task_switch();
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
	setup_fds(task);

	uint32_t current_eip = read_eip();
	if (current_task == parent) {
		task->eip = current_eip;
		return task;
	}

	task->state = RUNNABLE;
	task->wake_timestamp = 0;

	return task;
}

void add_process(task_t* task) {
	if (!tasking_installed()) return;
	array_m_insert(tasks, task);
}

void idle() {
	while (1) {}
}

void reap() {
	while (1) {
		for (int i = 0; i < blocked->size; i++) {
			task_t* task = array_m_lookup(blocked, i);
			if (task->state == ZOMBIE) {
				array_m_remove(blocked, i);
			}
		}
		//we have nothing else to do, yield cpu
		sys_yield(RUNNABLE);
	}
}

void iosent() {
	while (1) {
		update_blocked_tasks();
		//yield cpu to next task
		sys_yield(RUNNABLE);
	}
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
	setup_fds(kernel);
	
	current_task = kernel;
	array_m_insert(tasks, kernel);
	
	//create callback to switch tasks
	add_callback((void*)task_switch, 4, true, 0);

	//idle task
	//runs when anything (including kernel) is blocked for i/o
	if (!fork("idle")) {
		idle();
	}

	//task reaper
	//cleans up zombied tasks
	if (!fork("reaper")) {
		reap();
	}

	//blocked task sentinel
	//watches system events and wakes threads as necessary
	if (!fork("iosentinel")) {
		iosent();
	}

	//reenable interrupts
	kernel_end_critical();

	printf_info("Tasking initialized with kernel PID %d", getpid());
}

void update_blocked_tasks() {
	if (!tasking_installed()) return;

	kernel_begin_critical();

	for (int i = 0; i < blocked->size; i++) {
		task_t* task = array_m_lookup(blocked, i);
		if (task->state == PIT_WAIT) {
			if (time() >= task->wake_timestamp) {
				unblock_task(task);
				//goto_pid(task->id);
				//return;
			}
		}
		else if (task->state == KB_WAIT) {
			if (haskey()) {
				unblock_task(task);
				//goto_pid(task->id);
				//return;
			}
		}
	}

	kernel_end_critical();
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

	if (tasks->size < 1) {
		ASSERT(0, "next_runnable_task(): no runnable tasks!");
	}
	else if (tasks->size == 1) {
		//idle was the only available task
		task_t* idle = array_m_lookup(tasks, 0);
		return idle;
	}

	//find index of currently running task
	int idx = array_m_index(tasks, current_task);
	/*
	if (idx < 0) {
		ASSERT(0, "Task %s [%d] was executing but not marked as Runnable!", current_task->name, current_task->id);
	}
	*/
	//if this is the last index, loop around to the start of the array
	if (idx + 1 >= tasks->size || idx < 0) {
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
	bool found_task = false;
	for (int i = 0; i < tasks->size; i++) {
		task_t* tmp = array_m_lookup(tasks, i);
		if (tmp->id == id) {
			current_task = tmp;
			found_task = true;
			break;
		}
	}
	if (!found_task) {
		printf_err("Couldn't find non-blocked PID %d!", id);
		ASSERT(0, "Invalid context switch state");
	}

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	current_directory = current_task->page_dir;
	task_switch_real(eip, current_directory->physicalAddr, ebp, esp);
}

volatile uint32_t task_switch() {
	//check if idle is only available task
	if (tasks->size <= 1 || tasks->size >= MAX_TASKS) {
		//don't bother trying to find a task and context switching
		//idle can keep running
		return;
	}
	
	//find next runnable task
	task_t* next = next_runnable_task();
	//printf_dbg("task_switch(): switching to %s %d", next->name, next->id);
	goto_pid(next->id);
}

void _kill() {
	if (!tasking_installed()) return;

	kernel_begin_critical();
	block_task(current_task, ZOMBIE);
	kernel_end_critical();
}

void proc() {
	terminal_settextcolor(COLOR_WHITE);

	printf("-----------------------proc-----------------------\n");
	if (tasks->size) {
		printf("Active: \n");
		for (int i = 0; i < tasks->size; i++) {
			task_t* tmp = array_m_lookup(tasks, i);
			printf("[%d] %s\n", tmp->id, tmp->name);
			//printf("\tesp: %x ebp %x eip: %x cr3: %x\n", tmp->esp, tmp->ebp, tmp->eip, tmp->page_dir);
		}
	}
	else {
		printf("No active tasks\n");
	}
	if (blocked->size) {
		printf("Blocked: \n");
		for (int i = 0; i < blocked->size; i++) {
			task_t* tmp = array_m_lookup(blocked, i);
			printf("[%d] %s\n", tmp->id, tmp->name);
			//printf("\tesp: %x ebp %x eip: %x cr3: %x\n", tmp->esp, tmp->ebp, tmp->eip, tmp->page_dir);
			printf("\tBlocked for ");
			switch (tmp->state) {
				case KB_WAIT:
					printf("keyboard.");
					break;
				case PIT_WAIT:
					printf("timer (waking at timestamp %d)", tmp->wake_timestamp);
					break;
				default:
					printf("%d (unknown reason)", tmp->state);
					break;
			}
			printf("\n");
		}
	}
	else {
		printf("No blocked tasks.\n");
	}
	printf("---------------------------------------------------\n");
}
