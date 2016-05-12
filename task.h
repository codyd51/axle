#ifndef TASK_H
#define TASK_H

#include "common.h"
#include "paging.h"

#define KERNEL_STACK_SIZE 2048

//structure defines a 'task, or process
typedef struct task {
	int id; //process ID
	u32int esp, ebp; //stack and base pointers
	u32int eip; //instruction pointer
	page_directory_t* page_directory; //page directory
	struct task* next; //next task in linked list
} task_t;

//initializes tasking system
void initialize_tasking();

//called by timer hook, changes running process
void task_switch();

//forks current process, spawning new one with different memory space
int fork();

//causes current process' stack to be moved to a new location
void move_stack(void* new_stack_start, u32int size);

//returns PID of current process
int getpid();

#endif
