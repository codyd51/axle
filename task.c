#include "task.h"
#include "paging.h"
#include "std.h"

//currently running task
volatile task_t* current_task;

//start of the task linked list
volatile task_t* ready_queue;

//access members of paging.c
extern page_directory_t* kernel_directory;
extern page_directory_t* current_directory;
extern void alloc_frame(page_t*, int, int);
extern u32int initial_esp;
extern u32int read_eip();

//next available process ID
u32int next_pid = 1;

void initialize_tasking() {
	asm volatile("cli");

	//relocate stack so we know where it is
	move_stack((void*)0xE0000000, 0x2000);

	//initialize first task (kernel task)
	current_task = ready_queue = (task_t*)kmalloc(sizeof(task_t));
	current_task->id = next_pid++;
	current_task->esp = current_task->ebp = 0;
	current_task->eip = 0;
	current_task->page_directory = current_directory;
	current_task->next = 0;

	//reenable interrupts
	asm volatile("sti");
}

int fork() {
	//don't interrupt while modifying kernel structures
	asm volatile("cli");

	//take pointer to this process' task struct for later reference
	task_t* parent_task = (task_t*)current_task;

	//clone address space
	page_directory_t* directory = clone_directory(current_directory);

	//create new process
	task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
	new_task->id = next_pid++;
	new_task->esp = new_task->ebp = 0;
	new_task->eip = 0;
	new_task->page_directory = directory;
	new_task->next = 0;

	//add it to end of ready queue
	//find end of ready queue...
	task_t* tmp_task = (task_t*)ready_queue;
	while (tmp_task->next) tmp_task = tmp_task->next;
	//extend it
	tmp_task->next = new_task;
	
	//entry point for new process
	u32int eip = read_eip();

	//we could be parent or child here
	if (current_task == parent_task) {
		//we're the parent, so set up esp/ebp/eip for our child
		u32int esp; asm volatile("mov %%esp, %0" : "=r" (esp));
		u32int ebp; asm volatile("mov %%ebp, %0" : "=r" (ebp));
		new_task->esp = esp;
		new_task->ebp = ebp;
		new_task->eip = eip;
		
		//done! reenable interrupts
		asm volatile("sti");
		return new_task->id;
	}
	else {
		//we are child; by convention return 0
		return 0;
	}
}

int getpid() {
	return current_task->id;
}

void switch_task() {
	//if we haven't initialized yet, return
	if (!current_task) return;

	//disable interrupts
	asm volatile("cli");

	//read esp, ebp 
	u32int esp, ebp, eip;
	asm volatile("mov %%esp, %0" : "=r"(esp));
	asm volatile("mov %%ebp, %0" : "=r"(ebp));

	//read instruction pointer
	//one of 2 things could have happened when this function exits:
	//we called function and it returned EIP as expected
	//we just switched tasks and because saved EIP is essentially
	//the instruction after read_eip(), it will seem like read_eip
	//has just returned.
	//in the second case we need to return immediately
	//to detect it, hijack EAX 
	eip = read_eip();

	//did we just switch tasks?
	if (eip == 0x12345) return;

	//we did not switch tasks. Save register values and switch
	current_task->eip = eip;
	current_task->esp = esp;
	current_task->ebp = ebp;

	//get next task to run
	current_task = current_task->next;

	//if we fell off the end of the linked list, start again
	if (!current_task) current_task = ready_queue;

	esp = current_task->esp;
	ebp = current_task->ebp;

	perform_task_switch(eip, current_directory->physicalAddr, ebp, esp);

	//reenable interrupts
	asm volatile("sti");
}

void move_stack(void* new_stack_start, u32int size) {
	//allocate space for new stack
	for (u32int i = (u32int)new_stack_start; i >= ((u32int)new_stack_start - size); i -= 0x1000) {
		//general purpose stack is in user mode
		alloc_frame(get_page(i, 1, current_directory), 0 /*user mode*/, 1 /*writable*/);
	}
	
	//flush TLB by reading and writing page directory address again
	u32int pd_addr;
	asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
	asm volatile("mov %0, %%cr3" : : "r" (pd_addr));

	//old ESP and EBP; read from registers
	u32int old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
	u32int old_base_pointer; asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));

	u32int offset = (u32int)new_stack_start - initial_esp;

	u32int new_stack_pointer = old_stack_pointer + offset;
	u32int new_base_pointer = old_base_pointer + offset;

	//copy stack
	memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp - old_stack_pointer);

	//backtrace through original stack
	//copy values into new stack
	for (u32int i = (u32int)new_stack_start; i > (u32int)new_stack_start - size; i -= 4) {
		u32int* tmp = *(u32int*)i;
		//if value tmp is inside range of old stakc, assume it's a base pointer
		//and remap it. 
		//This will remap ANY value in this range, whether they're base pointers or not
		if ((old_stack_pointer < tmp) && (tmp < initial_esp)) {
			tmp = tmp + offset;
			u32int* tmp2 = (u32int*)i;
			*tmp2 = tmp;
		}
	}

	//change stacks
	asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
	asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
}


