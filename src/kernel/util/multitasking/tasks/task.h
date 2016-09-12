#ifndef TASK_H
#define TASK_H

#include <std/std.h>
#include <kernel/util/paging/paging.h>

#define KERNEL_STACK_SIZE 2048 //use 2kb kernel stack

enum { PRIO_LOW, PRIO_MED, PRIO_HIGH} PRIO;

typedef enum task_state {
    RUNNABLE = 0,
	ZOMBIE,
    KB_WAIT,
    PIT_WAIT,
} task_state;

typedef struct task {
	char* name;
	int id; 

	task_state state; //current process state 
    uint32_t wake_timestamp; //used if process is in PIT_WAIT state

	uint32_t esp; //stack pointer
	uint32_t ebp; //base pointer
	uint32_t eip; //instruction pointer

	page_directory_t* page_dir; //paging directory for this process
} task_t;

//initializes tasking system
void tasking_install();
bool tasking_installed();

//initialize a new process structure
//does not add returned process to running queue
//task_t* create_process(uint32_t eip);

//adds task to running queue
void add_process(task_t* task);

//changes running process
volatile uint32_t task_switch();

//forks current process
//spawns new process with different memory space
int fork();

//stop executing the current process and remove it from active processes
void _kill();

//used whenever a system event occurs
//looks at blocked tasks and unblocks as necessary
void update_blocked_tasks();

//returns pid of current process
int getpid();

#endif
