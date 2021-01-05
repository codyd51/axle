#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/vmm/vmm.h>
#include <kernel/util/spinlock/spinlock.h>

#define FD_MAX 64

typedef struct task_context {
	uint32_t ebp;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t eax;
	uint32_t eip;
} task_context_t;

typedef struct task_block_state {
	task_state status;
	uint32_t wake_timestamp; // used if process is in PIT_WAIT
} task_block_state_t;

typedef enum task_priority {
	// Idle task has the lowest possible priority
	PRIORITY_IDLE = 0,
	PRIORITY_NONE = 1,
	PRIORITY_DRIVER = 999,
	PRIORITY_TASK_RUNNING_ISR = 1000
} task_priority_t;

typedef struct task_small {
	uint32_t id;  // PID
	char* name; // user-printable process name
	task_context_t* machine_state; // registers at the time of last preemption
	task_block_state_t blocked_info; // runnable state
	struct task_small_t* next; // next task in linked list of all tasks

	uint32_t current_timeslice_start_date;
	uint32_t current_timeslice_end_date;

	uint32_t queue; //scheduler ring this task is slotted in
	uint32_t lifespan;

	bool is_thread;
	vmm_page_directory_t* vmm;

	array_l* fd_table;
	std_stream_t* stdin_stream;
	std_stream_t* stdout_stream;
	std_stream_t* stderr_stream;

	/*
	 * The following attributes are set only 
	 * for programs started via a loader,
	 * such as an ELF from the filesystem
	 */

	// End of allocated "program break" data (for sbrk)
	uint32_t sbrk_current_break;
	// Virtual address of the start of the .bss segmen
	uint32_t bss_segment_addr;
	uint32_t sbrk_current_page_head;

	task_priority_t priority;
	// Lock around modifying a task's priority
	spinlock_t priority_lock;
	// Meaning of this field is up to whatever sets the priority
	// Ex: If the task priority is PRIORITY_TASK_RUNNING_ISR,
	// i.e. this task is currently interrupted and executing an interrupt handler,
	// this field will contain the original priority to be reset when the ISR returns.
	uint32_t priority_context;
} task_small_t;

void tasking_init_small();
bool tasking_is_active();

void task_switch();

task_small_t* thread_spawn(void* entry_point);
task_small_t* task_spawn(void* entry_point, task_priority_t priority, const char* task_name);

task_small_t* tasking_get_task_with_pid(int pid);
task_small_t* tasking_get_current_task();

// Block a task because it must wait for the provided condition
// The task will eventually become unblocked either through a call to 
// tasking_unblock_task().
// tasking_unblock_task() may be called either from another part of the system,
// or from the iosentinel watchdog that notices that the block condition is 
// satisfied.
void tasking_block_task(task_small_t* task, task_state blocked_state);

void iosentinel_check_now();

// Query the active task
int getpid();
task_priority_t get_current_task_priority();

void tasking_print_processes(void);

#endif