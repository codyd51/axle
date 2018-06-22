#ifndef TASK_SMALL_H
#define TASK_SMALL_H

#include <stdint.h>
#include <kernel/multitasking/tasks/task.h>

typedef struct task_small {
	char* name; //user-printable process name
	int id;  //PID
    task_state state; // TODO(pt) change to task_wait_state?

	registers_t register_state;

	int queue; //scheduler ring this task is slotted in
    uint32_t wake_timestamp; //used if process is in PIT_WAIT state

	uint32_t begin_date;
	uint32_t end_date;

	uint32_t relinquish_date;
	uint32_t lifespan;
	struct task* next;
} task_small_t;

void tasking_init_small();

#endif