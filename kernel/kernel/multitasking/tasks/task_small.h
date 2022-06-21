#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/vmm/vmm.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/elf.h>
#include <std/array_l.h>

typedef enum task_state {
	UNKNOWN = 			(0 << 0),
    RUNNABLE = 			(1 << 0),
	// Intermediate state after task finishes executing before being flushed from system
	ZOMBIE = 			(1 << 1),
    KB_WAIT = 			(1 << 2),
    PIT_WAIT = 			(1 << 3),
	MOUSE_WAIT = 		(1 << 4),
	CHILD_WAIT = 		(1 << 5),
	PIPE_FULL = 		(1 << 6),
	PIPE_EMPTY = 		(1 << 7),
	IRQ_WAIT = 			(1 << 8),
	// The process has blocked until it receives an IPC message
	AMC_AWAIT_MESSAGE = (1 << 9),
	// Kernel code is modifying the
	// task's virtual address space
	VMM_MODIFY = 		(1 << 10),
	// AMC service sleeping until a timestamp has been reached
	AMC_AWAIT_TIMESTAMP = (1 << 11),
} task_state_t;

typedef struct task_context {
	uint64_t rbp;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbx;
	uint64_t rax;
	uint64_t rip;
} task_context_t;

typedef struct task_block_state {
	volatile task_state_t status;
	uint32_t wake_timestamp; // used if process is in TIMED_AWAIT_TIMESTAMP
	volatile task_state_t unblock_reason;
} task_block_state_t;

typedef struct task_small {
	uint32_t id;  // PID
	char* name; // user-printable process name
	task_context_t* machine_state; // registers at the time of last preemption
	task_block_state_t blocked_info; // runnable state
	struct task_small* next; // next task in linked list of all tasks

	uint64_t current_timeslice_start_date;
	uint64_t current_timeslice_end_date;

	uint32_t queue; //scheduler ring this task is slotted in
	uint32_t lifespan;

	bool is_thread;
	vas_state_t* vas_state;

	/*
	 * The following attributes are set only 
	 * for programs started via a loader,
	 * such as an ELF from the filesystem
	 */

	// End of allocated "program break" data (for sbrk)
	uintptr_t sbrk_current_break;
	// Virtual address of the start of the .bss segmen
	uintptr_t bss_segment_addr;
	uintptr_t sbrk_current_page_head;

	uintptr_t kernel_stack;
	uintptr_t kernel_stack_malloc_head;

	elf_t elf_symbol_table;
} task_small_t;

void tasking_init_small();
bool tasking_is_active();

void task_switch();
void tasking_goto_task(task_small_t* new_task, uint32_t quantum);
// Task switch only if the current task's quantum has expired
void task_switch_if_quantum_expired(void);

task_small_t* thread_spawn(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);
task_small_t* task_spawn(const char* task_name, void* entry_point);
task_small_t* task_spawn__with_args(const char* task_name, void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3);

task_small_t* tasking_get_task_with_pid(int pid);
task_small_t* tasking_get_current_task();

// Block a task because it must wait for the provided condition
// The task will eventually become unblocked either through a call to 
// tasking_unblock_task().
// tasking_unblock_task() may be called either from another part of the system,
// or from the iosentinel watchdog that notices that the block condition is 
// satisfied.
void tasking_block_task(task_small_t* task, task_state_t blocked_state);
void tasking_unblock_task_with_reason(task_small_t* task, task_state_t reason);

void iosentinel_check_now();

// Query the active task
int getpid();

void tasking_print_processes(void);

void tasking_disable_scheduling(void);
void tasking_reenable_scheduling(void);

void mlfq_goto_task(task_small_t* task);

void task_set_name(task_small_t* task, const char* new_name);

#endif