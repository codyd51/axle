#ifndef SCHED_RECORD_H
#define SCHED_RECORD_H

#include "task.h"
#include <stdint.h>

#define MAX_TASK_HISTORY 256
#define MAX_PROC_NAME	 64

typedef struct task_history {
	char history[MAX_TASK_HISTORY][MAX_PROC_NAME];
	int vals[MAX_TASK_HISTORY];
	int count;
	uint32_t time;
} task_history_t;

void sched_record_usage(task_t* current_task, uint32_t runtime);
void sched_log_history();
task_history_t* sched_get_task_history();

#endif
