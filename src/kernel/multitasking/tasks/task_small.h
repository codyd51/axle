#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/multitasking/tasks/task.h>
#include <kernel/vmm/vmm.h>

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
	vmm_pdir_t* vmm;
} task_small_t;

void tasking_init_small();
bool tasking_is_active();

void task_switch();

task_small_t* thread_spawn(void* entry_point);
task_small_t* task_spawn(void* entry_point);

#endif