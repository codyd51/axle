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
#include <gfx/lib/gfx.h>
#include <user/xserv/xserv.h>
#include <kernel/util/multitasking/pipe.h>
#include <kernel/util/multitasking/std_stream.h>
#include <kernel/util/multitasking/fd.h>
#include <kernel/util/shmem/shmem.h>

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

task_t* first_responder_task = 0;
static array_m* responder_stack = 0;

static lock_t* mutex = 0;

void enqueue_task(task_t* task, int queue);
void dequeue_task(task_t* task);

void stdin_read(char* buf, uint32_t count);
void stdout_read(char* buffer, uint32_t count);
void stderr_read(char* buffer, uint32_t count);
static void setup_fds(task_t* task) {
	memset(&task->fd_table, 0, sizeof(fd_entry) * FD_MAX);

	//initialize backing std stream
	task->std_stream = std_stream_create();

	//set up stdin/out/err to point to task's std stream
	//this stream backs all 3 descriptors
	fd_entry std;
	std.type = STD_TYPE;
	std.payload = task->std_stream;

	task->fd_table[0] = std;
	task->fd_table[1] = std;
	task->fd_table[2] = std;
}

task_t* task_list() {
	return active_list;
}

static bool is_dead_task_crit(task_t* task) {
	static char* crit_tasks[3] = {"idle",
								  "iosentinel"
	};

	for (uint32_t i = 0; i < sizeof(crit_tasks) / sizeof(crit_tasks[0]); i++) {
		if (!strcmp(crit_tasks[i], task->name)) {
			return true;
		}
	}
	return false;
}

static void tasking_critical_fail() {
	char* msg = "One or more critical tasks died. axle has died.\n";
	printf("%s\n", msg);
	//turn off interrupts
	kernel_begin_critical();
	//sleep until next interrupt (infinite loop)
	asm("hlt");
	//in case that ever finishes, infinite loop again
	while (1) {}
}

void kill_task(task_t* task) {
	bool show_died_message = !strcmp(task->name, "xserv");
	if (show_died_message) {
		xserv_fail();
	}

	if (is_dead_task_crit(task)) {
		tasking_critical_fail();
	}

	if (task == first_responder_task) {
		resign_first_responder();
	}
	block_task(task, ZOMBIE);
}

void _kill() {
	kill_task(current_task);
}

void goto_pid(int id, bool update_current_task_state);
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

void block_task_context(task_t* task, task_state reason, void* context) {
	if (!tasking_installed()) return;

	task->state = reason;
	task->block_context = context;

	//immediately switch tasks if active task was just blocked
	if (task == current_task) {
		task_switch(true);
	}
}

void block_task(task_t* task, task_state reason) {
	block_task_context(task, reason, NULL);
}

void unblock_task(task_t* task) {
	if (!tasking_installed()) return;

	lock(mutex);
	task->state = RUNNABLE;
	task->block_context = NULL;
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
	task->child_tasks = array_m_create(32);
	//task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
	setup_fds(task);

	uint32_t current_eip = read_eip();
	if (current_task == parent) {
		task->eip = current_eip;
		return task;
	}

	task->state = RUNNABLE;
	task->wake_timestamp = 0;
	task->vmem_slide = 0;
	task->windows = array_m_create(16);

	return task;
}
#pragma GCC diagnostic pop

task_t* task_with_pid_auth(int pid) {
	//first, ensure this task is allowed to do this!
	//permission to use task_with_pid is controlled by the PROC_MASTER_PERMISSION flag
	//only check if this is a non-kernel task
	//check for .bss segment as heuristic for whether this is an external program
	if (current_task->prog_break) {
		if (!(current_task->permissions & PROC_MASTER_PERMISSION)) {
			printf_err("%s[%d] is not authorized to use task_with_pid!", current_task->name, getpid());
			return NULL;
		}
	}
	//operation permitted
	return task_with_pid(pid);
}

task_t* task_with_pid(int pid) {
	task_t* tmp = active_list;
	while (tmp != NULL) {
		if (tmp->id == pid) {
			return tmp;
		}
		tmp = tmp->next;
	}
	return NULL;
}

task_t* task_current() {
	return current_task;
}

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
	if (task == first_responder_task) {
		resign_first_responder();
	}

	//close all pipes this process has opened
	for (int i = 0; i < FD_MAX; i++) {
		fd_entry entry = task->fd_table[i];
		if (fd_empty(entry)) continue;

		if (entry.type == PIPE_TYPE) {
			pipe_t* pipe = (pipe_t*)entry.payload;
			pipe_close(pipe->fd);
		}
	}

	//remove task from queues and active list
	unlist_task(task);
	//printf_info("%s[%d] destroyed.", task->name, task->id);
	//free task's page directory
	free_directory(task->page_dir);
	array_m_destroy(task->child_tasks);
	std_stream_destroy(task);

	kfree(task->name);
	kfree(task);
}

void reap_task(task_t* tmp) {
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

	printf_dbg("moving stack");
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

	printf_dbg("creating task queues");
	queues = array_m_create(queue_count + 1);
	for (int i = 0; i < queue_count; i++) {
		array_m* queue = array_m_create(MLFQ_MAX_QUEUE_LENGTH);
		array_m_insert(queues, queue);
	}

	queue_lifetimes = array_m_create(queue_count + 1);
	for (int i = 0; i < queue_count; i++) {
		array_m_insert(queue_lifetimes, (type_t)(HIGH_PRIO_QUANTUM * (i + 1)));
	}


	printf_dbg("setting up kernel task");
	//init first task (kernel task)
	task_t* kernel = kmalloc(sizeof(task_t));
	memset(kernel, 0, sizeof(task_t));
	strcpy(kernel->name, "kax");
	kernel->id = next_pid++;
	kernel->page_dir = current_directory;
	kernel->child_tasks = array_m_create(32);
	//kernel->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
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

	printf_dbg("forking system processes");
	//idle task
	//runs when anything (including kernel) is blocked for i/o
	if (!fork("idle")) {
		idle();
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
	/*
	if (haskey() && first_responder_task->state == KB_WAIT) {
		unblock_task(first_responder_task);
		goto_pid(first_responder_task->id);
	}
	*/

	//wake blocked tasks if the event they were blocked for has occurred
	//TODO is this optimizable?
	//don't look through every queue, use linked list of tasks
	task_t* task = active_list;
	while (task) {
		if (task->std_stream->buf->count && task->state == KB_WAIT) {
			unblock_task(task);
			goto_pid(task->id, true);
		}
		else if (task->state == PIT_WAIT) {
			if (time() >= task->wake_timestamp) {
				unblock_task(task);
			}
		}
		//TODO figure out when exactly tasks with MOUSE_WAIT should be unblocked
		else if (task->state == MOUSE_WAIT) {
			unblock_task(task);
			goto_pid(task->id, true);
		}
		else if (task->state == CHILD_WAIT) {
			//search if any of this task's children are zombies
			for (int i = 0; i < task->child_tasks->size; i++) {
				task_t* child = array_m_lookup(task->child_tasks, i);
				if (child->state == ZOMBIE) {
					//found a zombie!
					//wake parent
					unblock_task(task);
					break;
				}
			}
		}
		else if (task->state == PIPE_FULL) {
			pipe_block_info* info = (pipe_block_info*)task->block_context;
			pipe_t* waiting = info->pipe;
			int free_bytes = waiting->cb->capacity - waiting->cb->count;
			if (free_bytes >= info->free_bytes_needed) {
				//space has freed up in the pipe
				//we can now unblock
				unblock_task(task);
			}
		}
		else if (task->state == PIPE_EMPTY) {
			pipe_t* waiting = task->block_context;
			if (waiting->cb->count > 0) {
				//pipe now has data we can read
				//we can now unblock
				unblock_task(task);
			}
		}
		else if (task->state == ZOMBIE) {
			if (task->parent) {
				if (task->parent->state != CHILD_WAIT) {
					//printk("parent %d isn't waiting for dangling child %d\n", task->parent->id, task->id);
				}
			}
		}
		else if (task->state == IRQ_WAIT) {
			if (task->irq_satisfied) {
				task->irq_satisfied = false;
				unblock_task(task);
			}
		}

		task = task->next;
	}
}

void int_wait(int irq) {
	task_t* task = current_task;
	task->block_context = (void*)irq;
	task->irq_satisfied = false;
	block_task_context(task, IRQ_WAIT, (void*)INT_VECTOR_IRQ1);
}

task_t* first_responder() {
	return first_responder_task;
}

int fork(char* name) {
	if (!tasking_installed()) return 0; //TODO: check this result

	kernel_begin_critical();

	//keep reference to parent for later
	task_t* parent = current_task;

	task_t* child = create_process(name, 0, false);

	//copy all file descriptors from parent to child
	for (int i = 0; i < FD_MAX; i++) {
		fd_entry entry = parent->fd_table[i];
		if (fd_empty(entry)) continue;

		fd_add_index(child, entry, i);
		if (entry.type == PIPE_TYPE) {
			pipe_t* pipe = (pipe_t*)entry.payload;
			//and add this new child to the pipe's reference list
			array_m_insert(pipe->pids, (type_t)child->id);
		}
	}

	add_process(child);

	//set parent process of newly created process to currently running task
	child->parent = parent;
	//insert the newly created child task into the parent's array of children
	if (!parent->child_tasks) {
		ASSERT(0, "%s[%d] had no child_task array!\n", parent->name, parent->id);
	}
	if (parent->child_tasks->size < parent->child_tasks->max_size) {
		array_m_insert(parent->child_tasks, child);
	}
	else {
		ASSERT(0, "fork() child_tasks was full!\n");
	}

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

void goto_pid(int id, bool update_current_task_state) {
	if (!update_current_task_state) {
		//printk("goto_pid(%d %d)\n", id, update_current_task_state);
		update_current_task_state = 0;
	}

	if (!current_task || !queues) {
		return;
	}
	if (id == current_task->id) {
		//printk("called goto_pid with current_task->id %d, is this intentional?", id);
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
	if (update_current_task_state) {
		current_task->eip = eip;
		current_task->esp = esp;
		current_task->ebp = ebp;
	}

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
	//set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;
	current_directory = current_task->page_dir;
	task_switch_real(eip, current_directory->physicalAddr, ebp, esp);
}

uint32_t task_switch(bool update_current_task_state) {
	current_task->relinquish_date = time();
	//find next runnable task
	task_t* next = mlfq_schedule();

	ASSERT(next->state == RUNNABLE, "Tried to switch to non-runnable task %s (reason: %d)!", next->name, next->state);

	goto_pid(next->id, update_current_task_state);
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
		task_switch(true);
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
			printk("[%d Q %d] %s %s", task->id, task->queue, task->name, (task == first_responder()) ? "(FR)" : "");
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
				case CHILD_WAIT:
					printk("(blocked by child)");
					break;
				case PIPE_EMPTY:
				case PIPE_FULL:
					printk("(blocked by pipe)");
					break;
				case IRQ_WAIT:
					printk("(blocked by IRQ)");
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

void become_first_responder_pid(int pid) {
	task_t* task = task_with_pid(pid);
	if (!task) {
		printk("become_first_responder_pid(%d) failed\n", pid);
		return;
	}

	first_responder_task = task;

	//check if this task already exists in stack of responders
	for (int i = 0; i < responder_stack->size; i++) {
		task_t* tmp = array_m_lookup(responder_stack, i);
		if (tmp == first_responder_task) {
			//remove task so we can add it again
			//this is to ensure responder stack only has unique tasks
			array_m_remove(responder_stack, i);
		}
	}

	//append this task to stack of responders
	array_m_insert(responder_stack, first_responder_task);
}

void become_first_responder() {
	become_first_responder_pid(getpid());
}

void resign_first_responder() {
	if (!first_responder_task) return;
	//if (current_task != first_responder_task) return;

	//remove current first responder from stack of responders
	int last_idx = responder_stack->size - 1;
	task_t* removed = array_m_lookup(responder_stack, last_idx);
	ASSERT(removed == first_responder_task, "top of responder stack wasn't first responder!");

	array_m_remove(responder_stack, last_idx);

	if (responder_stack->size) {
		//set first responder to new head of stack
		first_responder_task = array_m_lookup(responder_stack, responder_stack->size - 1);
	}
	else {
		first_responder_task = NULL;
	}
}

void jump_user_mode() {
	// Set up a stack structure for switching to user mode.
	// the pop eax, or, and re-push take eflags which was pushed onto the stack,
	// and turns on the interrupt enabled flag
	// this ensures interrupts will be turned back on upon iret, as we do a cli at the
	// beginning of this routine, and can't do an sti once we're done since we're in user mode
	//set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);
	asm volatile("  \
			cli; \
			mov $0x23, %ax; \
			mov %ax, %ds; \
			mov %ax, %es; \
			mov %ax, %fs; \
			mov %ax, %gs; \
			\
			mov %esp, %eax; \
			pushl $0x23; \
			pushl %eax; \
			pushf; \
			pop %eax; \
			or %eax, 0x200; \
			push %eax; \
			pushl $0x1B; \
			push $1f; \
			iret; \
			1: \
			");
}

int waitpid(int pid, int* status, int options) {
	task_t* parent = current_task;
	block_task(parent, CHILD_WAIT);

	//wait finished!
	//find child which terminated
	for (int i = 0; i < parent->child_tasks->size; i++) {
		task_t* child = array_m_lookup(parent->child_tasks, i);
		//check if this pid is suitable to wake parent
		//if requested pid is -1, any child is acceptable
		//otherwise, we need exact match
		bool valid_pid = false;
		if (pid == -1 || pid == child->id) {
			valid_pid = true;
		}


		if (child->state == ZOMBIE && valid_pid) {
			int ret = child->exit_code;
			int child_pid = child->id;
			array_m_remove(parent->child_tasks, i);
			reap_task(child);

			if (status) {
				*status = ret;
			}

			//if pid is -1, then we are waiting for all child tasks to complete
			//so, if pid is -1 and there is another child process running,
			//keep waiting
			if (pid == -1 && parent->child_tasks->size) {
				return waitpid(pid, status, options);
			}

			return child_pid;
		}
	}
	ASSERT(0, "parent unblocked but no child terminated!\n");
	return -1;
}

int wait(int* status) {
	return waitpid(-1, status, 0);
}

Window* task_register_window(Rect frame) {
	//if we're creating a window for a task through xserv_win_create
	//then we're in a syscall handler and getpid() will return the pid of the
	//proc that ran the syscall
	//this is how we know when a user proc is connected to a window
	task_t* current = task_with_pid(getpid());
	Window* win = create_window(frame);
	array_m_insert(current->windows, win);

	return win;
}
