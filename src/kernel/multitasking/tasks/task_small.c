#include "task_small.h"

#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

static int next_pid = 1;

task_small_t* _current_task_small = 0;
task_small_t* _task_list_head = 0;

static lock_t* mutex = 0;

void new_task_entry() {
    while (1) {
    }
}

void new_my_task2() {
    while (1) {
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

void context_switch(registers_t* registers) {
    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_get_next_task(previous_task);

    //only overwrite preempted task's register state if it's been scheduled before and doesn't just contain setup values
    if (previous_task->_has_run) {
        //record machine state in previous_task
        memcpy(&(previous_task->register_state), registers, sizeof(registers_t));
    }

    //copy machine state into what will be restored when IRQ exits
    if (!next_task->_has_run) {
        //next_task is a newly constructed task
        //its register_state doesn't contain actual machine state, only setup values
        registers->eip = next_task->register_state.eip;
        registers->esp = next_task->register_state.esp;
        registers->ebp = next_task->register_state.ebp;
    }
    else {
        //next_task has run in the past and has real state to restore
        memcpy(registers, &(next_task->register_state), sizeof(registers_t));
    }

    _current_task_small = next_task;
    next_task->_has_run = true;

    printf("%d\n", _current_task_small->id);
}

static void _tasking_add_task_to_runlist(task_small_t* task) {
    if (!_current_task_small) {
        printf_dbg("first runlist task");
        _current_task_small = task;
        return;
    }
    task_small_t* list_tail = _tasking_last_task_in_runlist();
    printf_dbg("last task is pid %d", list_tail->id);
    list_tail->next = task;
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
    new_task->_has_run = false;

    _tasking_add_task_to_runlist(new_task);

    return new_task;
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

void tasking_init_small() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }
    kernel_begin_critical();

    printf_info("Multitasking init...");
    mutex = lock_create();
    add_callback((void*)context_switch, 200, true, 0);

    //init first task (kernel task)
    _current_task_small = task_construct((uint32_t)&new_task_entry);
    _task_list_head = _current_task_small;
    //init another
    task_small_t* buddy = task_construct((uint32_t)&new_my_task2);

    printf_info("Tasking initialized with kernel PID %d", getpid());
    kernel_end_critical();
}
