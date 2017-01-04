#include "record.h"
#include <kernel/drivers/pit/pit.h>
#include <std/math.h>

//array of tasks with usage history
static char task_history[MAX_TASK_HISTORY][MAX_PROC_NAME];
//array of corresponding runtimes to tasks
static int task_history_vals[MAX_TASK_HISTORY];
//keep track of current # of tasks with history
static int current_taskcount = 0;

void sched_record_usage(task_t* current_task, uint32_t runtime) {
	if (!current_task) return;

	//if first run, memset arrays
	if (!current_taskcount) {
		memset(task_history, 0, sizeof(task_history));
		memset(task_history_vals, 0, sizeof(task_history_vals));
	}
	//are we about to exceed array bounds?
	if (current_taskcount + 1 >= MAX_TASK_HISTORY) {
		printk("sched_record_usage() exceeds array\n");
		while (1) {}
	}

	//search if this task already has usage history
	char* line = 0;
	int idx = -1;
	for (int i = 0; i < current_taskcount; i++) {
		line = (char*)task_history[i];

		//is this a match?
		if (strcmp(current_task->name, line) == 0) {
			idx = i;
			break;
		}
	}

	//did this user already exist?
	if (idx == -1) {
		idx = current_taskcount;
		strcpy(task_history[current_taskcount++], current_task->name);
	}
	task_history_vals[idx] += runtime;
}

void sched_log_history() {
	//find length of longest task name to align output
	int longest_len = 0;
	for (int i = 0; i < current_taskcount; i++) {
		int curr_len = strlen(task_history[i]);
		longest_len = MAX(longest_len, curr_len);
	}

	uint32_t uptime = tick_count();
	printk("\n---CPU usage history---\n");
	printk("%d total ticks\n", uptime);
	for (int i = 0; i < current_taskcount; i++) {
		//print name of task
		printk("%s used", task_history[i]);

		//print spaces needed to align output with longest task name
		int diff = longest_len - strlen(task_history[i]);
		for (int j = 0; j < diff; j++) {
			printk(" ");
		}

		float cpu_usage = (float)(task_history_vals[i]) / (float)uptime;
		//out of 100, not out of 1
		cpu_usage *= 100;
		printk(" %f%% of total CPU time\n", cpu_usage);
	}
	printk("-----------------------\n");
}

task_history_t* sched_get_task_history() {
	task_history_t* ret = kmalloc(sizeof(task_history_t));
	memcpy(ret->history, task_history, sizeof(task_history));
	memcpy(ret->vals, task_history_vals, sizeof(task_history_vals));
	ret->count = current_taskcount;
	ret->time = tick_count();
	return ret;
}

