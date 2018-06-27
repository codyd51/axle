#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/multitasking/tasks/task.h>

typedef struct task_context {
	uint32_t ebp;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t eax;
	uint32_t eip;
} task_context_t;

typedef struct task_small {
	uint32_t id;  //PID
	char* name; //user-printable process name
	task_context_t* machine_state; //registers at the time of last preemption
    task_state waiting_state; // TODO(pt) change to task_wait_state?
	struct task_small_t* next; // next task in linked list of all tasks

	uint32_t current_timeslice_start_date;
	uint32_t current_timeslice_end_date;

	uint32_t queue; //scheduler ring this task is slotted in
	uint32_t relinquish_date;
	uint32_t lifespan;

    uint32_t wake_timestamp; //used if process is in PIT_WAIT state
} task_small_t;

void tasking_init_small();
void task_switch_now();
bool tasking_is_active();

#endif