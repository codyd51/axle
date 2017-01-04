#include "task.h"
#include <std/std.h>
#include <std/math.h>
#include <std/memory.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/util/paging/descriptor_tables.h>
#include <kernel/util/paging/paging.h>
#include <kernel/util/multitasking/util.h>
#include <kernel/util/syscall/sysfuncs.h>
#include <kernel/drivers/rtc/clock.h>
#include <std/klog.h>
#include <kernel/util/mutex/mutex.h>
#include "record.h"

//function defined in asm which returns the current instruction pointer
uint32_t read_eip();
//defined in asm
//performs actual task switch
void task_switch_real(uint32_t eip, uint32_t paging_dir, uint32_t ebp, uint32_t esp);

//magic value placed in eax at end of task switch
//we read eax when trying to catch current eip
//if this value is in eax, we know we already caught eip and that the task switch is over, so it should return immediately
#define STACK_MAGIC 0xDEADBEEF

#define MAX_TASKS 128
#define MAX_FILES 32

#define MLFQ_DEFAULT_QUEUE_COUNT 16
#define MLFQ_MAX_QUEUE_LENGTH 16

#define HIGH_PRIO_QUANTUM 5
#define BOOSTER_PERIOD 1000

#define MAX_RESPONDERS 32

extern page_directory_t* current_directory;

static int next_pid = 1;
task_t* current_task = 0;
static array_m* queues = 0;
static array_m* queue_lifetimes = 0;
static task_t* active_list = 0;

task_t* first_responder = 0;
static array_m* responder_stack = 0;

static lock_t* mutex = 0;

void enqueue_task(task_t* task, int queue);
void dequeue_task(task_t* task);

void stdin_read(char* buf, uint32_t count);
void stdout_read(char* buffer, uint32_t count);
void stderr_read(char* buffer, uint32_t count);
static void setup_fds(task_t* task) { task->files = array_m_create(MAX_FILES);
	// array_m_insert(task->files, stdin_read);
	// array_m_insert(task->files, stdout_read);
	// array_m_insert(task->files, stderr_read);
}

static void kill(task_t* task) {
	if (!tasking_installed()) return;

	//TODO only go back to terminal mode if task we're killing changed gfx mode
	//instead of hard coding these
	printk("_kill() checking if need to change gfx mode for task %s\n", task->name);
	if (!strcmp(task->name, "xserv") || !strcmp(task->name, "rexle")) {
		printk("_kill() switching back to terminal mode\n");
		switch_to_text();
	}
	block_task(task, ZOMBIE);
}

void _kill() {
	block_task(current_task, ZOMBIE);
	reap();
	//kill(current_task);
}

void goto_pid(int id);
int getpid() {
	if (current_task) {
		return current_task->id;
	}
	return -1;
}

void unlist_task(task_t* task) {
	//if task to unlist is head, move head
	if (task == active_list) {
		active_list = task->next;
	}
	else {
		//walk linked list
		task_t* prev = active_list;
		task_t* current = prev;
		while (current && current->next != NULL) {
			if (current == task) {
				break;
			}
			prev = current;
			current = current->next;
		}
		//did we find it?
		if (task != current) {
			printk("unlist_task() couldn't unlist %s\n", task->name);
			return;
		}

		//remove from list
		prev->next = current->next;
	}
}

void list_task(task_t* task) {
	//walk linked list
	task_t* current = active_list;
	while (current->next != NULL) {
		if (task == current) {
			return;
		}
		current = current->next;
	}

	//extend list
	current->next = task;
}

void block_task(task_t* task, task_state reason) {
	if (!tasking_installed()) return;

	task->state = reason;

	//immediately switch tasks if active task was just blocked
	if (task == current_task) {
		task_switch();
	}
}

void unblock_task(task_t* task) {
	if (!tasking_installed()) return;

	lock(mutex);
	task->state = RUNNABLE;
	unlock(mutex);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
task_t* create_process(char* name, uint32_t eip, bool wants_stack) {
	task_t* parent = current_task;

	//clone address space
	page_directory_t* cloned = clone_directory(current_directory);

	//create new process
	task_t* task = kmalloc(sizeof(task_t));
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
#pragma GCC diagnostic pop

void add_process(task_t* task) {
	if (!tasking_installed()) return;

	list_task(task);

	//all new tasks are placed on highest priority queue
	enqueue_task(task, 0);
}

void idle() {
	while (1) {
		//nothing to do!
		//put the CPU to sleep until the next interrupt
		asm volatile("hlt");
		//once we return from above, go to next task
		sys_yield(RUNNABLE);
	}
}

void destroy_task(task_t* task) {
	//remove task from queues and active list
	unlist_task(task);
	//free task's page directory
	//free_directory(task->page_dir);
}

void reap() {
	while (1) {
		task_t* tmp = active_list;
		while (tmp != NULL) {
			if (tmp->state == ZOMBIE) {
				array_m* queue = array_m_lookup(queues, tmp->queue);
				int idx = array_m_index(queue, tmp);
				if (idx != ARR_NOT_FOUND) {
					printk("reap() unlisting %s\n", tmp->name);

					lock(mutex);
					array_m_remove(queue, idx);
					unlock(mutex);

					destroy_task(tmp);
				}
				else {
					//couldn't find task in the queue it said it was in
					//fall back on searching through each queue
					bool found = false;
					for (int i = 0; i < queues->size && !found; i++) {
						array_m* queue = array_m_lookup(queues, i);
						for (int j = 0; j < queues->size && !found; j++) {
							task_t* to_test = array_m_lookup(queue, j);
							if (to_test == tmp) {
								lock(mutex);
								array_m_remove(queue, j);
								unlock(mutex);

								destroy_task(tmp);
								found = true;
								break;
							}
						}
					}
					if (!found) {
						printf_err("Tried to reap task %s[%d] but it didn't exist in a queue", tmp->name, tmp->id);
					}
				}
			}
			tmp = tmp->next;
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

void enqueue_task(task_t* task, int queue) {
	lock(mutex);
	if (queue < 0 || queue >= queues->size) {
		ASSERT(0, "Tried to insert %s into invalid queue %d", task->name, queue);
	}

	array_m* raw = array_m_lookup(queues, queue);

	//ensure task does not already exist in this queue
	if (array_m_index(raw, task) == ARR_NOT_FOUND) {
		lock(mutex);
		array_m_insert(raw, task);
		unlock(mutex);

		task->queue = queue;
		//new queue, reset lifespan
		task->lifespan = 0;
	}
	else {
		printf_err("Tried to enqueue %s onto queue where it already existed (%d)", task->name, queue);
	}
	unlock(mutex);
}

void dequeue_task(task_t* task) {
	lock(mutex);
	if (task->queue < 0 || task->queue >= queues->size) {
		ASSERT(0, "Tried to remove %s from invalid queue %d", task->name, task->queue);
	}
	array_m* raw = array_m_lookup(queues, task->queue);

	int idx = array_m_index(raw, task);
	if (idx < 0) {
		printf_err("Tried to dequeue %s from queue %d it didn't belong to!", task->name, task->queue);
		//fall back on searching all queues for this task
		for (int i = 0; i < queues->size; i++) {
			array_m* queue = array_m_lookup(queues, i);
			for (int j = 0; j < queue->size; j++) {
				task_t* tmp = array_m_lookup(queue, j);
				if (task == tmp) {
					//found task we were looking for
					printf_info("Task was actually in queue %d", i);
					array_m_remove(queue, j);
					unlock(mutex);

					return;
				}
			}
		}
		//never found the task!
		printf_err("Task %s did not exist in any queues!", task->name);
		return;
	}

	array_m_remove(raw, idx);
	unlock(mutex);

	//if for some reason this task is still in the queue (if it was added to queue twice),
	//dequeue it again
	if (array_m_index(raw, task) != ARR_NOT_FOUND) {
		dequeue_task(task);
	}
}

void switch_queue(task_t* task, int new) {
	dequeue_task(task);
	enqueue_task(task, new);
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
	return (queues && queues->size >= 1 && current_task);
}

void booster() {
	task_t* tmp = active_list;
	while (tmp) {
		switch_queue(tmp, 0);
		tmp = tmp->next;
	}
}

void tasking_install(mlfq_option options) {
	if (tasking_installed()) return;

	printf_info("Initializing tasking...");

	kernel_begin_critical();

	printk_info("moving stack...");
	move_stack((void*)0xE0000000, 0x2000);
	printk_info("moved stack\n");

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

	queue_lifetimes = array_m_create(queue_count + 1);
	for (int i = 0; i < queue_count; i++) {
		array_m_insert(queue_lifetimes, (type_t)(HIGH_PRIO_QUANTUM * (i + 1)));
	}

	printk("queues\n");

	//init first task (kernel task)
	task_t* kernel = kmalloc(sizeof(task_t));
	memset(kernel, 0, sizeof(task_t));
	kernel->name = "kax";
	kernel->id = next_pid++;
	kernel->page_dir = current_directory;
	setup_fds(kernel);

	current_task = kernel;
	active_list = kernel;
	enqueue_task(current_task, 0);

	//set up responder stack
	responder_stack = array_m_create(MAX_RESPONDERS);
	//set kernel as initial first responder
	become_first_responder();

	//create callback to switch tasks
	void handle_pit_tick();
	add_callback((void*)handle_pit_tick, 4, true, 0);

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

	mutex = lock_create();

	//reenable interrupts
	kernel_end_critical();

	printf_info("Tasking initialized with kernel PID %d", getpid());
}

void update_blocked_tasks() {
	if (!tasking_installed()) return;

	//if there is a pending key, wake first responder
	if (haskey() && first_responder->state == KB_WAIT) {
		unblock_task(first_responder);
		goto_pid(first_responder->id);
	}

	//wake blocked tasks if the event they were blocked for has occurred
	//TODO is this optimizable?
	//don't look through every queue, use linked list of tasks
	task_t* task = active_list;
	while (task) {
		if (task->state == PIT_WAIT) {
			if (time() >= task->wake_timestamp) {
				unblock_task(task);
			}
		}

		//TODO figure out when exactly tasks with MOUSE_WAIT should be unblocked
		if (task->state == MOUSE_WAIT) {
			unblock_task(task);
			goto_pid(task->id);
		}

		task = task->next;
	}
}

int fork(char* name) {
	if (!tasking_installed()) return 0; //TODO: check this result

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
		kernel_end_critical();
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
	//we could look at every queue individually, but that would be slow
	//let's take advantage of our linked list of tasks and search that
	task_t* curr = active_list;
	task_t* highest_prio_runnable = NULL;

	//TODO figure out why this block doesn't work
	while (curr) {
		if (curr->state == RUNNABLE) {
			//if this task has a higher priority (lower queue #), or this is the first runnable task we've found,
			//mark it as best
			if (!highest_prio_runnable || curr->queue < highest_prio_runnable->queue) {
				highest_prio_runnable = curr;
			}
		}
		curr = curr->next;
	}

	array_m* queue = array_m_lookup(queues, highest_prio_runnable->queue);
	if (!highest_prio_runnable || highest_prio_runnable->state != RUNNABLE || !queue->size) {
	//if (1) {
		//printf_err("Couldn't find runnable task in linked list of tasks!");
		for (int i = 0; i < queues->size; i++) {
			array_m* tmp = array_m_lookup(queues, i);
			if (first_queue_runnable(tmp, 0) != NULL) {
				return tmp;
			}
		}
	}
	//no queues contained any runnable tasks!
	if (!highest_prio_runnable) {
		proc();
		ASSERT(highest_prio_runnable, "No queues contained any runnable tasks!");
	}

	return queue;
}

task_t* mlfq_schedule() {
	if (!tasking_installed()) return NULL;

	//find current index in queue
	array_m* current_queue = array_m_lookup(queues, current_task->queue);
	int current_task_idx = array_m_index(current_queue, current_task);
	if (current_task_idx < 0) {
		ASSERT(0, "Couldn't find current task in queue %d", current_task->queue);
	}

	//increment lifespan by how long this task ran
	if (current_task->relinquish_date && current_task->begin_date) {
		uint32_t current_runtime = (current_task->relinquish_date - current_task->begin_date);
		current_task->lifespan += current_runtime;
		sched_record_usage(current_task, current_runtime);
	}

	if (current_task->lifespan >= (uint32_t)array_m_lookup(queue_lifetimes, current_task->queue)) {
		demote_task(current_task);
	}

	//if we're running in low-latency mode, save time by just using round-robin
	if (queues->size == 1) {
		//attempt to save time by first looking at the next task in linked list
		task_t* next = current_task->next;
		if (!next) next = active_list;
		while (next->state != RUNNABLE) {
			next = next->next;
			if (!next) {
				next = active_list;
			}
		}
		ASSERT(next, "Couldn't find valid runnable task!");
		return next;
	}

	//find first non-empty queue
	array_m* new_queue = first_queue_containing_runnable();
	if (!new_queue->size) {
		proc();
	}
	ASSERT(new_queue->size, "Couldn't find any queues with tasks to run in queue %d!", array_m_index(queues, new_queue));

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
	if (id == current_task->id) {
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
	int lifetime = (int)array_m_lookup(queue_lifetimes, current_task->queue);
	current_task->end_date = current_task->begin_date + lifetime;

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	current_directory = current_task->page_dir;
	task_switch_real(eip, current_directory->physicalAddr, ebp, esp);
}

uint32_t task_switch() {
	current_task->relinquish_date = time();
	//find next runnable task
	task_t* next = mlfq_schedule();

	ASSERT(next->state == RUNNABLE, "Tried to switch to non-runnable task %s (reason: %d)!", next->name, next->state);

	goto_pid(next->id);
	//TODO: what should be returned here?
	return 0;
}

void handle_pit_tick() {
	static uint32_t tick = 0;
	static uint32_t last_boost = 0;

	if (!tick) {
		//first run
		//get real time
		tick = time();
		last_boost = tick;
		return;
	}

	//due to an apparant bug in the PIT callback mechanism,
	//having a callback every tick introduces bugs and triple faults
	//going as fast as every other tick does not have this problem
	//it seems as if the bug happens if we don't finish the tick interrupt before the next interrupt fires
	//to be safe, this is only called once every 4 ticks
	//so, we need to increment tick count by 4 ticks
	tick += 4;
	if (tick >= current_task->end_date) {
		task_switch();
	}
	if (tick >= last_boost + BOOSTER_PERIOD) {
		//don't boost if we're in low latency mode!
		if (queues->size > 1) {
			last_boost = tick;
			booster();
		}
	}
}

void proc() {
	printk("-----------------------proc-----------------------\n");

	for (int i = 0; i < queues->size; i++) {
		array_m* queue = array_m_lookup(queues, i);
		for (int j = 0; j < queue->size; j++) {
			task_t* task = array_m_lookup(queue, j);
			uint32_t runtime = (uint32_t)array_m_lookup(queue_lifetimes, task->queue);
			printk("[%d Q %d] %s ", task->id, task->queue, task->name);
			if (task == current_task) {
				printk("(active)");
			}
			else {
				printk("used");
			}
			printk(" %d/%d ms ", task->lifespan, runtime);

			switch (task->state) {
				case RUNNABLE:
					printk("(runnable)");
					break;
				case KB_WAIT:
					printk("(blocked by keyboard)");
					break;
				case PIT_WAIT:
					printk("(blocked by timer, wakes %d)", task->wake_timestamp);
					break;
				case MOUSE_WAIT:
					printk("(blocked by mouse)");
					break;
				case ZOMBIE:
					printk("(zombie)");
					break;
				default:
					break;
		}
			printk("\n");
		}
	}
	printk("---------------------------------------------------\n");
}

void force_enumerate_blocked() {
	if (!tasking_installed()) return;

	update_blocked_tasks();
}

void become_first_responder() {
	first_responder = current_task;

	//check if this task already exists in stack of responders
	for (int i = 0; i < responder_stack->size; i++) {
		task_t* tmp = array_m_lookup(responder_stack, i);
		if (tmp == first_responder) {
			//remove task so we can add it again
			//this is to ensure responder stack only has unique tasks
			array_m_remove(responder_stack, i);
		}
	}

	//append this task to stack of responders
	array_m_insert(responder_stack, first_responder);
}

void resign_first_responder() {
	//remove current first responder from stack of responders
	int last_idx = responder_stack->size - 1;
	array_m_remove(responder_stack, last_idx);
	//set first responder to new head of stack
	first_responder = array_m_lookup(responder_stack, responder_stack->size - 1);
}

