#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/multitasking/tasks/task.h>

struct context {
	uint32_t ebp;
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t eax;
	uint32_t eip;
};

typedef struct task_small {
	char* name; //user-printable process name
	int id;  //PID
    task_state state; // TODO(pt) change to task_wait_state?

	registers_t register_state;

	int queue; //scheduler ring this task is slotted in
    uint32_t wake_timestamp; //used if process is in PIT_WAIT state

	uint32_t current_timeslice_start_date;
	uint32_t current_timeslice_end_date;

	uint32_t relinquish_date;
	uint32_t lifespan;
	struct task_small_t* next;

    bool _has_run; //has the task ever been scheduled?

	struct context* context;
} task_small_t;

void tasking_init_small();
void task_switch_now();
bool tasking_is_active();

#endif