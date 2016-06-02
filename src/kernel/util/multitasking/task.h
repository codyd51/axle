#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <kernel/util/interrupts/isr.h>
#include <kernel/util/paging/paging.h>

#define KERNEL_STACK_SIZE 2048 //use 2kb kernel stack

enum { PRIO_LOW, PRIO_MED, PRIO_HIGH} PRIO;

//describes a process
typedef struct task {
	int id; //process id
	uint32_t esp, ebp; //stack and base pointers
	uint32_t eip; //instruction pointer
	page_directory_t* page_directory; 
	uint32_t kernel_stack; //kernel stack location
	struct task* next; //next task in linked list
	int tickets; //tickets this task holds for lottery
	int priority; //priority associated with this task
	char yielded; //did this task yield the last time it was ran?
} task_t;

//initializes tasking system
void tasking_install();

//called by timer
//changes running process
void task_switch(char yielded);

//forks current process
//spawns new process with different memory space
int fork(int priority);

//causes current process' stack to be forcibly moved to different location
void move_stack(void* new_stack_start, uint32_t start);

//returns pid of current process
int getpid();

#endif
