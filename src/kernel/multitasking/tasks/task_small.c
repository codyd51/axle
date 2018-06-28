#include "task_small.h"

#include <std/timer.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

#define TASK_QUANTUM 20
#define MAX_TASKS 64

static int next_pid = 1;

task_small_t* _current_task_small = 0;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);
static task_small_t* _task_list_head = 0;
static timer_callback_t* pit_callback = 0;

static lock_t* mutex = 0;

//defined in asm
//performs actual task switch
void task_switch_real(uint32_t eip, uint32_t paging_dir, uint32_t ebp, uint32_t esp);
void switch_real(uint32_t esp);
void task_entry();

void task2() {
    while (1) {
        printf("B");
        task_switch();
    }
}
void task3() {
    while (1) {
        printf("C");
        task_switch();
    }
}

void task_new(int i) {
    while (1) {
        printf("%d", getpid());
        sys_yield(RUNNABLE);
    }
}

void task_sleepy() {
    sleep(2000);
    printf("slept!\n");
    while (1) {}
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
    for (int i = 0; i < MAX_TASKS; i++) {
        if ((iter)->next == NULL) {
            return iter;
        }
        iter = (iter)->next;
    }
    panic("more than MAX_TASKS tasks in runlist. increase me?");
    return NULL;
}

static void _tasking_add_task_to_runlist(task_small_t* task) {
    if (!_current_task_small) {
        _current_task_small = task;
        return;
    }
    task_small_t* list_tail = _tasking_last_task_in_runlist();
    list_tail->next = task;
}

task_small_t* task_construct(void* entry_point, void* arg1) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;

    uint32_t stack_size = 0x1000;
    char *stack = kmalloc(stack_size);

    uint32_t *stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack
    *(stack_top--) = arg1;          //argument to entry point
    *(stack_top--) = entry_point;   //address of task's entry point
    *(stack_top--) = 0;             //eax
    *(stack_top--) = 0;             //ebx
    *(stack_top--) = 0;             //esi
    *(stack_top--) = 0;             //edi
    *(stack_top)   = 0;             //ebp

    new_task->machine_state = (task_context_t*)stack_top;

    _tasking_add_task_to_runlist(new_task);
}

void task_switch() {
    /*
    Immediately preempt the running task
    */
    void context_switch(uint32_t* new_task);

    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_get_next_task(previous_task);

    printf("|");
    // this method will update _current_task_small
    // this method performs the actual context switch and also updates _current_task_small
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

static void task_timer_tick() {
    task_switch();
    /*
    if (time() >= _current_task_small->current_timeslice_end_date) {
        //task_switch_from_pit(registers);
    }
    */
}

void tasking_init() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }
    kernel_begin_critical();

    mutex = lock_create();
    pit_callback = timer_callback_register((void*)task_timer_tick, 1, true, 0);

    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = task_construct(NULL, NULL);
    _task_list_head = _current_task_small;
    //init another
    //task_small_t* buddy = task_construct((uint32_t)&task2, NULL);
    //task_small_t* buddy1 = task_construct((uint32_t)&task_sleepy, NULL);
    for (int i = 0; i < MAX_TASKS; i++) {
        task_construct((uint32_t)task_new, i);
    }

    printf_info("Multitasking initialized");
    kernel_end_critical();
}
