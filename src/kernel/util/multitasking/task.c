#include "task.h"
#include <std/std.h>
#include <std/memory.h>

#define STACK_MAGIC 0xDEADBEEF

//currently running task
volatile task_t* current_task;

//start of task linked list
volatile task_t* ready_queue;

//access members of paging.c
extern page_directory_t* kernel_directory;
extern page_directory_t* current_directory;
extern void alloc_frame(page_t*, int, int);
extern uint32_t initial_esp;
extern uint32_t read_eip();

extern void perform_task_switch(uint32_t, uint32_t, uint32_t, uint32_t);

//next available pid
uint32_t next_pid = 1;

static void switch_callback() {
	switch_task();
}

task_t* create_process(int priority) {
	task_t* task = (task_t*)kmalloc(sizeof(task_t));
	task->priority = priority;
	task->id = next_pid++;
	task->esp = task->ebp = 0;
	task->eip = 0;
	task->next = 0;
	//assign tickets based on priority
	switch (priority) {
		case PRIO_HIGH:
			task->tickets = 100;
			break;
		case PRIO_MED:
			task->tickets = 50;
			break;
		case PRIO_LOW:
		default:
			task->tickets = 25;
			break;
	}

	//add task to end of ready queue
	//find end of ready queue
	if (ready_queue) {
		task_t* tmp = (task_t*)ready_queue;
		while (tmp->next) {
			tmp = tmp->next;
		}
		//extend it
		tmp->next = task;
	}

	return task;
}

void tasking_install() {
	printf_info("Initializing tasking...");
	
	asm volatile("cli");
	
	//relocate stack so we know where it is
	move_stack((void*)0xE0000000, 0x2000);

	//init first task (kernel task)
	current_task = ready_queue = create_process(PRIO_HIGH);
	current_task->page_directory = current_directory;
	current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);

	//create callback to switch tasks
	add_callback(switch_callback, 1, 1, 0);

	//reenable interrupts
	asm volatile("sti");

	sleep(1);
}

int fork(int priority) {
	asm volatile("cli");

	//take pointer to this process' task for later reference
	task_t* parent_task = (task_t*)current_task;

	//clone address space
	page_directory_t* directory = clone_directory(current_directory);

	//create new process
	task_t* task = create_process(parent_task->priority);
	task->page_directory = directory;
	current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);

	//entry point for new process
	uint32_t eip = read_eip();

	//we could be parent or child here! check
	if (current_task == parent_task) {
		//we are parent, so set up esp/ebp/eip for child
		uint32_t esp;
		asm volatile("mov %%esp, %0" : "=r" (esp));
		uint32_t ebp;
		asm volatile("mov %%ebp, %0" : "=r" (ebp));
		task->esp = esp;
		task->ebp = ebp;
		task->eip = eip;
		//all finished! reenable interrupts
		asm volatile("sti");

		int count = 1;
		task_t* tmp = ready_queue;
		while (tmp->next) {
			tmp = tmp->next;
			count++;
		}

		return task->id;
	}
	else {
		//we're the child
		//return 0 by convention
		asm volatile("sti");
		return 0;
	}
}

task_t* scheduler_lottery() {
	//find total number of tickets in existence
	int num_tickets = 0;
	task_t* tmp = ready_queue;
	do {
		num_tickets += tmp->tickets;
	} while((tmp = tmp->next));

	static int i = 0;
	i++;
	if (i == 1000) {
		i = 0;
	//	printf("num_tickets: %d\n", num_tickets);
	}

	//generate winning ticket of this lottery
	int winning_ticket = rand() % (num_tickets + 1);
	//find task owning winning ticket
	int ticket_counter = 0;
	tmp = ready_queue;
	do {
		ticket_counter += tmp->tickets;
		if (ticket_counter >= winning_ticket) break;
	} while ((tmp = tmp->next));

	return tmp;
}

void switch_task() {
	//if we haven't initialized tasking yet, just return
	if (!current_task) {
		return;
	}

	//read esp, ebp for saving later on
	uint32_t esp, ebp, eip;
	asm volatile("mov %%esp, %0" : "=r"(esp));
	asm volatile("mov %%ebp, %0" : "=r"(ebp));

	//read instruction pointer
	//one of 2 things could have happened when this function exits:
	//we called function and it returned eip as requested
	//or, we just switched tasks, and because saved eip is essentially
	//the instruction after read_eip(), it'll seem as if read_eip just returned
	//in the second case we need to return immediately
	//to detect it, put dummy value in eax
	eip = read_eip();

	//did we just switch tasks?
	if (eip == STACK_MAGIC) return;

	//did not switch tasks
	//save register values and switch
	current_task->eip = eip;
	current_task->esp = esp;
	current_task->ebp = ebp;

	//set current_task to lottery winner
	current_task = scheduler_lottery();

	eip = current_task->eip;
	esp = current_task->esp;
	ebp = current_task->ebp;

	current_directory = current_task->page_directory;

	//switch over kernel stack
	set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);

	static int count = 0;
	static int count2 = 0;
	if (current_task->id == 1) count++;
	else count2++;

	if (count > 1000 || count2 > 1000) {
		printf("count: %d\n", count);
		printf("count2: %d\n", count2);

		count = 0;
		count2 = 0;
	}

	//stop interrupts
	//temporarily put new eip in ecx
	//load stack and base ptrs from new task
	//change page directory to physical addr of new directory
	//put dummy value in eax so we can detect task switch
	//restart interrupts
	//jump to location in ecx (new eip is stored there)
	perform_task_switch(eip, current_directory->physicalAddr, ebp, esp);
}

void move_stack(void* new_stack_start, uint32_t size) {
	//allocate space for new stack
	for (int i = (uint32_t)new_stack_start; i >= ((uint32_t)new_stack_start - size); i -= 0x1000) {
		//general purpose stack is user mode
		alloc_frame(get_page(i, 1, current_directory), 0, 1);
	}

	//flush TLB by reading and writing page directory address again
	uint32_t pd_addr;
	asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
	asm volatile("mov %0, %%cr3" : : "r" (pd_addr));

	//old ESP and EBP
	uint32_t old_sp;
	asm volatile("mov %%esp, %0" : "=r" (old_sp));
	uint32_t old_bp;
	asm volatile("mov %%ebp, %0" : "=r" (old_bp));

	//offset to add to old stack addresses to get new stack address
	uint32_t offset = (uint32_t)new_stack_start - initial_esp;

	//new esp and ebp
	uint32_t new_sp = old_sp + offset;
	uint32_t new_bp = old_bp + offset;

	//copy stack!
	memcpy((void*)new_sp, (void*)old_sp, initial_esp - old_sp);

	//backtrace through original stack, copying new values into new stack
	for (int i = (uint32_t)new_stack_start; i > (uint32_t)new_stack_start - size; i -= 4) {
		uint32_t tmp = *(uint32_t*)i;
		//if value of tmp is inside range of old stack, 
		//assume it's a base pointer and remap it
		//TODO keep in mind this will remap ANY value in this range,
		//whether it's a base pointer or not
		if ((old_sp < tmp) && (tmp < initial_esp)) {
			tmp = tmp + offset;
			uint32_t* tmp2 = (uint32_t*)i;
			*tmp2 = tmp;
		}
	}

	//change stacks
	asm volatile("mov %0, %%esp" : : "r" (new_sp));
	asm volatile("mov %0, %%ebp" : : "r" (new_bp));
}

int getpid() {
	return current_task->id;
}

void switch_to_user_mode() {
	//set up kernel stack
	set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);

	//set up stack to switch to user mode	
	asm volatile("		\
		cli;		\
		mov $0x23, %ax;	\
		mov %ax, %ds;	\
		mov %ax, %es;	\
		mov %ax, %fs;	\
		mov %ax, %gs;	\
				\
		\
		mov %esp, %eax;	\
		pushl $0x23;	\
		pushl %esp;	\
		pushf;		\
		pushl $0x1B;	\
		push $1f;	\
		iret;		\
	    1: 	\
	    	");	
}
