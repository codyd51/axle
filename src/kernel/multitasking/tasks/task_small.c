#include "task_small.h"

#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

static int next_pid = 1;

task_small_t* _current_task_small = 0;
task_small_t** _task_list_head = &_current_task_small;

static lock_t* mutex = 0;

void new_task_entry() {
    printf("Hello!\n");
    while (1) {}
}

static task_small_t* _tasking_get_next_task(task_small_t* previous_task) {
    //pick tasks in round-robin
    task_small_t* next_task = previous_task->next;
    //end of list?
    if (!next_task) {
        next_task = *_task_list_head;
    }
    return next_task;
}

static task_small_t* _tasking_last_task_in_runlist() {
    task_small_t* iter = _task_list_head;
    for (int i = 0; i < 16; i++) {
        if (iter->next == NULL) {
            return iter;
        }
        iter = iter->next;
    }
    panic("more than 16 tasks in runlist. increase me?");
    return NULL;
}

void context_switch(registers_t* registers) {
    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_get_next_task(previous_task);
    printf_info("switching from PID %d to PID %d", previous_task->id, next_task->id);

    //record machine state in previous_task
    memcpy(&(previous_task->register_state), registers, sizeof(registers_t));

    printf_dbg("old eip, esp: 0x%x, 0x%x", previous_task->register_state.eip, previous_task->register_state.esp);
    printf_dbg("new eip, esp: 0x%x, 0x%x", next_task->register_state.eip, next_task->register_state.esp);

    //copy machine state into what will be restored when IRQ exits
    memcpy(registers, &(next_task->register_state), sizeof(registers_t));
    _current_task_small = next_task;
}

task_small_t* task_construct(uint32_t entry_point) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;

    registers_t initial_register_state = {0};
    initial_register_state.ds = GDT_BYTE_INDEX_KERNEL_DATA;
    initial_register_state.eip = entry_point;

    char* stack = kmalloc(0x1000);
    initial_register_state.esp = stack;
    initial_register_state.ebp = stack;

    new_task->register_state = initial_register_state;

    return new_task;
}

void tasking_init_small() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }
    kernel_begin_critical();

    printf_info("Multitasking init...");

    //init first task (kernel task)
    _current_task_small = task_construct((uint32_t)&new_task_entry);
    add_callback((void*)context_switch, 4, true, 0);

    mutex = lock_create();

    printf_info("Tasking initialized with kernel PID %d", getpid());

    kernel_end_critical();
}
