#ifndef TASK_H
#define TASK_H

#include <std/std.h>
#include <kernel/util/paging/paging.h>

#define KERNEL_STACK_SIZE 2048 //use 2kb kernel stack

enum { PRIO_LOW, PRIO_MED, PRIO_HIGH} PRIO;

typedef enum task_state {
    RUNNABLE = 0,
    KB_WAIT,
    PIT_WAIT,
} task_state;

typedef struct context {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t ebp;
	uint32_t eip;
} context;

typedef struct task {
	bool switched_once;
	int id; //process id

	context* context;

	uint32_t esp;
	uint32_t ebp;
	uint32_t eip;
	uint32_t* kernel_stack;

	volatile registers_t regs; //register state
	page_directory_t* page_directory; //paging dir for this process
	struct task* next; //next task in linked list

    task_state state; //current process state 
    uint32_t wake_timestamp; //used if process is in PIT_WAIT state
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
