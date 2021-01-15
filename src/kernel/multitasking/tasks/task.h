#ifndef TASK_H
#define TASK_H

#include <std/std.h>
#include <kernel/util/paging/paging.h>
#include <std/array_l.h>
#include <kernel/multitasking/fd_entry.h>
//#include <kernel/multitasking//std_stream.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/rect.h>
#include <gfx/lib/window.h>
#include <kernel/util/ipc/ipc.h>
#include <gfx/lib/surface.h>

#define KERNEL_STACK_SIZE 4096 //use 4kb kernel stack
#define FD_MAX 64

//if a task has PROC_MASTER_PERMISSION set,
//it is allowed to use task_with_pid
//this flag is set through a program's Info.plist
//<param proc_master="allow"/>
#define PROC_MASTER_PERMISSION 1 << 0

typedef enum task_state {
	UNKNOWN = 			(0 << 0),
    RUNNABLE = 			(1 << 0),
	// Intermediate state after task finishes executing before being flushed from system
	ZOMBIE = 			(1 << 1),
    KB_WAIT = 			(1 << 2),
    PIT_WAIT = 			(1 << 3),
	MOUSE_WAIT = 		(1 << 4),
	CHILD_WAIT = 		(1 << 5),
	PIPE_FULL = 		(1 << 6),
	PIPE_EMPTY = 		(1 << 7),
	IRQ_WAIT = 			(1 << 8),
	// The process has blocked until it receives an IPC message
	AMC_AWAIT_MESSAGE = (1 << 9),
	// Kernel code is modifying the
	// task's virtual address space
	VMM_MODIFY = 		(1 << 10),
} task_state;

typedef enum mlfq_option {
	LOW_LATENCY = 0, //minimize latency between tasks running, tasks share a single queue
	PRIORITIZE_INTERACTIVE, //use more queues, allowing interactive tasks to dominate
} mlfq_option;

struct fd_entry;
typedef struct task {
	char* name; //user-printable process name
	int id;  //PID
	int queue; //scheduler ring this task is slotted in

	task_state state; //current process state
    uint32_t wake_timestamp; //used if process is in PIT_WAIT state

	uint32_t begin_date;
	uint32_t end_date;

	uint32_t relinquish_date;
	uint32_t lifespan;
	struct task* next;

	uint32_t esp; //stack pointer
	uint32_t ebp; //base pointer
	uint32_t eip; //instruction pointer

	page_directory_t* page_dir; //paging directory for this process


	/*
	 * the below only exist for non-kernel tasks
	 * (such as loaded ELFs)
	 */

	//end of .bss section of current task
	uint32_t prog_break;
	//virtual address of .bss segment
	uint32_t bss_loc;

	/* array of child tasks this process has spawned
	 * each time a process fork()'s,
	 * the new child is added to this array
	 * when wait() is used, it uses the tasks in this array
	 */
	array_m* child_tasks;
	//parent process that spawned this one
	struct task* parent;

	//exit status of zombie task
	//this field is undefined until task finishes executing
	int exit_code;

	//TODO move this near task_state and make clean
	//optional context provided with blocking reason
	//up to user what this means
	void* block_context;

	//file descriptor table
	//this stores all types of file descriptors,
	//including stdin/out/err, open files, and pipes
	fd_entry_t fd_table[FD_MAX];

	//pseudo-terminal stream
	//this field provides implementation for
	//stdin/stdout/stderr
	//(all of these map to the same backing stream)
	struct std_stream* std_stream;

	//virtual memory 'slide'
	//offset in virtual memory where this program is to be placed
	uint32_t vmem_slide;

	//bitmap of privileged actions this task can perform
	uint32_t permissions;

	//array of xserv windows this task has spawned
	//this is so we know where to send stdio to
	array_m* windows;

	bool irq_satisfied;
	ipc_state_t* ipc_state;

	char* kernel_stack;
} task_t;

//initializes tasking system
void tasking_init();
bool tasking_is_active();

void block_task(task_t* task, task_state reason);
void block_task_context(task_t* task, task_state reason, void* context);

//initialize a new process structure
//does not add returned process to running queue
task_t* create_process(char* name, uint32_t eip, bool wants_stack);

//adds task to running queue
void add_process(task_t* task);

//changes running process
uint32_t task_switch_old(bool update_current_task_state);

//forks current process
//spawns new process with different memory space
int fork();

//stop executing the current process and remove it from active processes
void _kill();

//kill task associated with task struct
void kill_task(task_t* task);

//used whenever a system event occurs
//looks at blocked tasks and unblocks as necessary
void update_blocked_tasks();

//returns pid of current process
int getpid();

//print all active processes
void proc();

//used to immediately invoke iosentinel process
//to wake any processes that were waiting on an i/o event that
//has now been recieved
void force_enumerate_blocked();

//internal function to query current task holding first responder status
//the first responder of axle receives all keyboard, mouse events out of
//tasks waiting for keystrokes
task_t* first_responder();

//appends current task to stack of responders,
//and marks current task as designated recipient of keyboard events
void become_first_responder();

//performs the same actions as become_first_responder(),
//but operates on the task with PID 'pid' instead of the currently running task
void become_first_responder_pid(int pid);

//relinquish first responder status
//process which first responder status was taken from becomes first responder
void resign_first_responder();

//find task_t associated with PID 'pid'
//returns NULL if no task_t with given PID exists
task_t* task_with_pid(int pid);
task_t* task_with_pid_auth(int pid);
task_t* task_current();

//suspend execution until child process terminates
int waitpid(int pid, int* status, int options);
int wait(int* status);

//utility function to retrieve head of linked list of tasks
task_t* task_list();

//create window with frame 'frame',
//and add to current task's list of registered windows
Window* task_register_window(Rect frame);

void task_register_surface(Surface* s, char* kernel_base);

void move_stack(void* new_stack_start, uint32_t size);

#endif
