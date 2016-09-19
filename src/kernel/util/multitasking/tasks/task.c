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

#define MLFQ_DEFAULT_QUEUE_COUNT 8
#define MLFQ_MAX_QUEUE_LENGTH 16

#define QUANTUM 20
#define BOOSTER_PERIOD 1000

extern page_directory_t* current_directory;

static int next_pid = 1;
task_t* current_task;
static array_m* queues;
static task_t* active_list;

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

void unlist_task(task_t* task) {
	//walk linked list
	task_t* prev = active_list;
	task_t* current = prev->next;
	while (current->next != NULL) {
		if (current == task) {
			break;
		}
		prev = current;
		current = current->next;
	}
	//did we find it?
	if (task != current) {
		//printf_err("[%d] %s unlist failed, wasn't listed", task->id, task->name);
		return;
	}

	//remove from list
	prev->next = current->next;
}

void list_task(task_t* task) {
	//walk linked list
	task_t* current = active_list;
	while (current->next != NULL) {
		if (task == current) {
			//printf_err("[%d] %s was already present in active list", task->id, task->name);
			return;
		}
		current = current->next;
	}

	//extend list
	current->next = task;
}

void block_task(task_t* task, task_state reason) {
	if (!tasking_installed()) return;

	kernel_begin_critical();
	task->state = reason;
	kernel_end_critical();

	//immediately switch tasks if active task was just blocked
	if (task == current_task) {
		task_switch();
	}
}

void unblock_task(task_t* task) {
	if (!tasking_installed()) return;

	kernel_begin_critical();
	task->state = RUNNABLE;
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

	list_task(task);

	//all new tasks are placed on highest priority queue
	enqueue_task(task, 0);
}

void idle() {
	while (1) {}
}

void reap() {
	while (1) {
		kernel_begin_critical();
		
		task_t* tmp = active_list;
		while (tmp != NULL) {
			if (tmp->state == ZOMBIE) {
				array_m* queue = array_m_lookup(queues, tmp->queue);
				array_m_remove(queue, array_m_index(queue, tmp));
				unlist_task(tmp);
			}
			tmp = tmp->next;
		}
	
		/*
		for (int i = 0; i < queues->size; i++) {
			array_m* queue = array_m_lookup(queues, i);
			for (int j = 0; j < queue->size; j++) {
				task_t* task = array_m_lookup(queue, j);
				if (task->state == ZOMBIE) {
					array_m_remove(queue, j);
					unlist_task(task);
				}
			}
		}
		*/
		kernel_end_critical();
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

void enqueue_task(task_t* task, int queue) {
	kernel_begin_critical();
	if (queue < 0 || queue >= queues->size) {
		ASSERT(0, "Tried to insert %s into invalid queue %d", task->name, queue);
	}
	array_m* raw = array_m_lookup(queues, queue);
	array_m_insert(raw, task);
	task->queue = queue;
	kernel_end_critical();
}

void dequeue_task(task_t* task) {
	kernel_begin_critical();
	if (task->queue < 0 || task->queue >= queues->size) {
		ASSERT(0, "Tried to remove %s from invalid queue %d", task->name, task->queue);
	}
	array_m* raw = array_m_lookup(queues, task->queue);
	int idx = array_m_index(raw, task);
	if (idx < 0) {
		ASSERT(0, "Tried to dequeue %s from queue %d it didn't belong to!", task->name, task->queue);
	}
	array_m_remove(raw, idx);
	kernel_end_critical();
}

void switch_queue(task_t* task, int new) {
	kernel_begin_critical();
	dequeue_task(task);
	enqueue_task(task, new);
	kernel_end_critical();
}

void demote_task(task_t* task) {
	//if we're already at the bottom task, don't attempt to demote further
	if (task->queue >= queues->size - 1) {
		return;
	}
	switch_queue(task, task->queue + 1);
}

void promote_task(task_t* task) {
	switch_queue(task, task->queue - 1);
}

bool tasking_installed() {
	return (queues->size >= 1 && current_task);
}

void booster() {
	kernel_begin_critical();
	for (int i = 0; i < queues->size; i++) {
		array_m* queue = array_m_lookup(queues, i);
		for (int j = 0; j < queue->size; j++) {
			task_t* tmp = array_m_lookup(queue, j);
			switch_queue(tmp, 0);
		}
	}
	kernel_end_critical();
}

void tasking_install(mlfq_option options) {
	if (tasking_installed()) return;

	printf_info("Initializing tasking...");
	
	kernel_begin_critical();

	move_stack((void*)0xE0000000, 0x2000);

	int queue_count = 0;
	switch (options) {
		case LOW_LATENCY:
			queue_count = 1;
			break;
		case PRIORITIZE_INTERACTIVE:
		default:
			queue_count = MLFQ_DEFAULT_QUEUE_COUNT;
			break;
	}

	queues = array_m_create(queue_count + 1);
	for (int i = 0; i < queue_count; i++) {
		array_m* queue = array_m_create(MLFQ_MAX_QUEUE_LENGTH);
		array_m_insert(queues, queue);
	}
	
	//init first task (kernel task)
	task_t* kernel = (task_t*)kmalloc(sizeof(task_t));
	memset(kernel, 0, sizeof(task_t));
	kernel->name = "kax";
	kernel->id = next_pid++;
	kernel->page_dir = current_directory;
	setup_fds(kernel);
	
	current_task = kernel;
	active_list = kernel;
	enqueue_task(current_task, 0);
	
	//create callback to switch tasks
	add_callback((void*)task_switch, QUANTUM, true, 0);
	add_callback(booster, BOOSTER_PERIOD, true, 0);

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
	
	//TODO is this optimizable?
	for (int i = 0; i < queues->size; i++) {
		array_m* tmp = array_m_lookup(queues, i);
		for (int j = 0; j < tmp->size; j++) {
			task_t* task = array_m_lookup(tmp, j);
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
					goto_pid(task->id);
					//return;
				}
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

task_t* first_queue_runnable(array_m* queue, int offset) {
	for (int i = offset; i < queue->size; i++) {
		task_t* tmp = array_m_lookup(queue, i);
		if (tmp->state == RUNNABLE) {
			return tmp;
		}
	}
	//no runnable tasks within this queue!
	return NULL;
}

array_m* first_queue_containing_runnable(void) {
	for (int i = 0; i < queues->size; i++) {
		array_m* tmp = array_m_lookup(queues, i);
		if (first_queue_runnable(tmp, 0) != NULL) {
			return tmp;
		}
	}
	//no queues contained any runnable tasks!
	ASSERT(0, "No queues contained any runnable tasks!");
}

task_t* mlfq_schedule() {
	if (!tasking_installed()) return;

	//find current index in queue
	array_m* current_queue = array_m_lookup(queues, current_task->queue);
	int current_task_idx = array_m_index(current_queue, current_task);
	if (current_task_idx < 0) {
		ASSERT(0, "Couldn't find current task in queue %d", current_task->queue);
	}

	//increment lifespan by how long this task ran
	if (current_task->relinquish_date && current_task->begin_date) {
		current_task->lifespan += (current_task->relinquish_date - current_task->begin_date);
	}
	
	if (current_task->lifespan >= QUANTUM) {
		current_task->lifespan = 0;
		demote_task(current_task);
	}

	//find first non-empty queue
	array_m* new_queue = first_queue_containing_runnable();
	ASSERT(new_queue->size, "Couldn't find any queues with tasks to run!");

	if (new_queue->size >= 1) {
		//round-robin through this queue
		
		//if this is the same queue as the previous task, start at that index
		if (current_queue == new_queue) {
			//if this is the last index, loop around to the start of the array
			if (current_task_idx + 1 >= new_queue->size) {
				task_t* valid = first_queue_runnable(new_queue, 0);
				if (valid != NULL) {
					return valid;
				}
			}
			//return task at the next index
			task_t* valid = first_queue_runnable(new_queue, current_task_idx + 1);
			if (valid != NULL) {
				return valid;
			}
		}

		//we're on a new queue
		//start from the first task in it
		task_t* valid = first_queue_runnable(new_queue, 0);
		if (valid != NULL) {
			return valid;
		}
	}
	ASSERT(0, "Couldn't find task to switch to!");
}

void goto_pid(int id) {
	if (!current_task || !queues) {
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

	//find task with this PID
	bool found_task = false;
	task_t* tmp = active_list;
	while (tmp != NULL) {
		if (tmp->id == id && tmp->state == RUNNABLE) {
			//switch to PID passed to us
			current_task = tmp;
			found_task = true;
			break;
		}
		tmp = tmp->next;
	}
	
	if (!found_task) {
		printf_err("PID %d wasn't in active list, falling back on queue search", id);
		//fall back on searching through each queue for this task
		for (int i = 0; i < queues->size; i++) {
			array_m* tasks = array_m_lookup(queues, i);
			for (int j = 0; j < tasks->size; j++) {
				task_t* tmp = array_m_lookup(tasks, j);
				if (tmp->id == id) {
					current_task = tmp;
					found_task = true;
					break;
				}
			}
		}

		//did we still not find it?
		if (!found_task) {
			printf_err("goto_pid: Nonexistant PID %d!", id);
			ASSERT(0, "Invalid context switch state");
		}
	}

	current_task->begin_date = time();

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	current_directory = current_task->page_dir;
	task_switch_real(eip, current_directory->physicalAddr, ebp, esp);
}

volatile uint32_t task_switch() {
	kernel_begin_critical();

	current_task->relinquish_date = time();
	//find next runnable task
	task_t* next = mlfq_schedule();
	ASSERT(next->state == RUNNABLE, "Tried to switch to non-runnable task %s (reason: %d)!", next->name, next->state);

	kernel_end_critical();

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

	for (int i = 0; i < queues->size; i++) {
		array_m* queue = array_m_lookup(queues, i);
		for (int j = 0; j < queue->size; j++) {
			task_t* task = array_m_lookup(queue, j);
			float age = (task->lifespan / (float)QUANTUM);
			printf("[%d] %s (queue %d - used %d/%d ms) ", task->id, task->name, task->queue, task->lifespan, QUANTUM);
			switch (task->state) {
				case RUNNABLE:
					printf("(runnable)");
					break;
				case KB_WAIT:
					printf("(blocked by keyboard)");
					break;
				case PIT_WAIT:
					printf("(blocked by timer, wakes %d)", task->wake_timestamp);
					break;
				default:
					break;
		}
			printf("\n");
		}
	}
	printf("---------------------------------------------------\n");
}
