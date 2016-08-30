#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include <kernel/util/paging/paging.h>

#define KERNEL_STACK_SIZE 2048 //use 2kb kernel stack

enum { PRIO_LOW, PRIO_MED, PRIO_HIGH} PRIO;

typedef struct task {
	int id; //process id
	volatile registers_t regs; //register state
	page_directory_t* page_directory; //paging dir for this process
	struct task* next; //next task in linked list
} task_t;

//initializes tasking system
void tasking_install();

//initialize a new process structure
//does not add returned process to running queue
task_t* create_process(uint32_t eip);

//adds task to running queue
void add_process(task_t* task);

//called by timer
//changes running process
volatile void task_switch();

//forks current process
//spawns new process with different memory space
int fork(int priority);

//returns pid of current process
int getpid();

#endif
