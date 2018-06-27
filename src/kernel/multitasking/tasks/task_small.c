#include "task_small.h"

#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>
#include <std/timer.h>

#define TASK_QUANTUM 20

static int next_pid = 1;

const int task_small_offset_to_context = offsetof(struct task_small, context);
task_small_t* _current_task_small = 0;
static task_small_t* _task_list_head = 0;
static timer_callback_t* pit_callback = 0;

static lock_t* mutex = 0;

//defined in asm
//performs actual task switch
void task_switch_real(uint32_t eip, uint32_t paging_dir, uint32_t ebp, uint32_t esp);
void switch_real(uint32_t esp);
void task_entry();

void new_task_entry() {
    while (1) {
        printf("~");
    }
}

static task_small_t* _tasking_get_next_task(task_small_t* previous_task) {
    //pick tasks in round-robin
    task_small_t* next_task = previous_task->next;
    //end of list?
    if (next_task == NULL) {
        next_task = _task_list_head;
    }
    return next_task;
}

static task_small_t* _tasking_last_task_in_runlist() {
    if (!_current_task_small) {
        return NULL;
    }
    task_small_t* iter = _current_task_small;
    for (int i = 0; i < 16; i++) {
        if ((iter)->next == NULL) {
            return iter;
        }
        iter = (iter)->next;
    }
    panic("more than 16 tasks in runlist. increase me?");
    return NULL;
}

void task_switch_now() {
    /*
    Immediately preempt the running task.
    */
    //set the process's time left to run to zero
    //set the time to next schedule to 0
    //task_switch_from_pit will be called on the next PIT interrupt
    _current_task_small->current_timeslice_end_date = time();
    timer_deliver_immediately(pit_callback);
    //put CPU to sleep until the next interrupt
    asm("hlt");
}

static void _tasking_add_task_to_runlist(task_small_t* task) {
    if (!_current_task_small) {
        _current_task_small = task;
        return;
    }
    task_small_t* list_tail = _tasking_last_task_in_runlist();
    list_tail->next = task;
}

task_small_t* task_construct(uint32_t entry_point) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;

    uint32_t stack_size = 0x2000;
    char *stack = kmalloc(stack_size);
    uint32_t *stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack

    *(stack_top--) = entry_point;      //Address of task's startup function
    *(stack_top--) = 0;                //eax
    *(stack_top--) = 0;                //ebx
    *(stack_top--) = 0;                //esi
    *(stack_top--) = 0;                //edi
    *(stack_top)   = 0;                //ebp

    new_task->context = (struct context*)stack_top;

    _tasking_add_task_to_runlist(new_task);
}

void task_switch_new() {
    void context_switch(uint32_t* new_task);

    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_get_next_task(previous_task);

    //will update _current_task_small
    printf("S");
    context_switch(next_task);
}

int getpid() {
    if (!_current_task_small) {
        return -1;
    }
    return _current_task_small->id;
}

bool tasking_is_active() {
    //return (queues && queues->size >= 1 && current_task);
    return _current_task_small != 0;
}

static void scheduler_tick(registers_t* registers) {
    if (time() >= _current_task_small->current_timeslice_end_date) {
        //task_switch_from_pit(registers);
    }
}

void tasking_init() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }
    kernel_begin_critical();

    printf_info("Multitasking init...");
    mutex = lock_create();
    //pit_callback = add_callback((void*)scheduler_tick, 5, true, 0);

    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = task_construct((uint32_t)&new_task_entry);
    _task_list_head = _current_task_small;
    //init another
    task_small_t* buddy = task_construct((uint32_t)&new_task_entry);
    //task_small_t* buddy1 = task_construct((uint32_t)&new_my_task3);

    printf_info("Multitasking initialized");
    kernel_end_critical();
}

void access_context(struct task_small t) {
    t.context = 1;
}