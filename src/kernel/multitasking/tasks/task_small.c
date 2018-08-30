#include "task_small.h"

#include <std/timer.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/segmentation/gdt_structures.h>

#define TASK_QUANTUM 10
#define MAX_TASKS 64

static int next_pid = 1;

task_small_t* _current_task_small = 0;
static task_small_t* _task_list_head = 0;

static timer_callback_t* pit_callback = 0;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);

// defined in process_small.s
// performs the actual context switch
void context_switch(uint32_t* new_task);

void task_new() {
    while (1) {
        printf("%d", getpid());
    }
}

void task_sleepy() {
    sleep(2000);
    printf("slept!\n");
    while (1) {}
}

void sleep(uint32_t ms) {
    _current_task_small->blocked_info.status = PIT_WAIT;
    _current_task_small->blocked_info.wake_timestamp = time() + ms;
    task_switch();
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

static task_small_t* _tasking_get_next_runnable_task(task_small_t* previous_task) {
    task_small_t* iter = _tasking_get_next_task(previous_task);
    for (int i = 0; i < MAX_TASKS; i++) {
        if (iter->blocked_info.status != RUNNABLE) {
            iter = _tasking_get_next_task(iter);
            continue;
        }
        return iter;
    }
    panic("couldn't find runnable task");
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

task_small_t* tasking_get_task_with_pid(int pid) {
    if (!_current_task_small) {
        return NULL;
    }
    task_small_t* iter = _current_task_small;
    for (int i = 0; i < MAX_TASKS; i++) {
        if ((iter)->id == pid) {
            return iter;
        }
        iter = (iter)->next;
    }
    //not found
    return NULL;
}

task_small_t* tasking_get_current_task() {
    return tasking_get_task_with_pid(getpid());
}

task_small_t* thread_spawn_with_machine_state(task_context_t* state) {
    NotImplemented();
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;
    new_task->blocked_info.status = RUNNABLE;

    uint32_t stack_size = 0x1000;
    char *stack = kmalloc(stack_size);

    // XXX(PT): this doesn't work because we'd need to clone the entire stack of the old task
    uint32_t *stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack
    //*(stack_top--) = state->esp;   //address of task's entry point
    *(stack_top--) = 0;             //eax
    *(stack_top--) = 0;             //ebx
    *(stack_top--) = 0;             //esi
    *(stack_top--) = 0;             //edi
    *(stack_top)   = 0;             //ebp

    new_task->machine_state = (task_context_t*)stack_top;

    new_task->is_thread = true;
    new_task->vmm = vmm_active_pdir();

    _tasking_add_task_to_runlist(new_task);
}

task_small_t* thread_spawn(void* entry_point) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;
    new_task->blocked_info.status = RUNNABLE;

    uint32_t stack_size = 0x1000;
    char *stack = kmalloc(stack_size);
    memset(stack, 0, stack_size);

    uint32_t *stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack
    *(stack_top--) = entry_point;   //address of task's entry point
    *(stack_top--) = 0;             //eax
    *(stack_top--) = 0;             //ebx
    *(stack_top--) = 0;             //esi
    *(stack_top--) = 0;             //edi
    *(stack_top)   = 0;             //ebp

    new_task->machine_state = (task_context_t*)stack_top;

    new_task->is_thread = true;
    new_task->vmm = vmm_active_pdir();

    _tasking_add_task_to_runlist(new_task);
}

task_small_t* task_spawn(void* entry_point) {
    task_small_t* new_task = thread_spawn(entry_point);
    new_task->is_thread = false;
    // a task is simply a thread with its own virtual address space
    // the new task's address space is a clone of the task that spawned it
    new_task->vmm = vmm_clone_active_pdir();
    return new_task;
}
}

/*
 * Immediately preempt the running task and begin running the provided one.
 */
void tasking_goto_task(task_small_t* new_task) {

    uint32_t now = time();
    new_task->current_timeslice_start_date = now;
    new_task->current_timeslice_end_date = now + TASK_QUANTUM;

    if (new_task->vmm != _current_task_small->vmm) {
        vmm_load_pdir(new_task->vmm);
    }
    // this method will update _current_task_small
    // this method performs the actual context switch and also updates _current_task_small
    context_switch(new_task);
}

/*
 * Pick the next task to schedule, and preempt the currently running one.
 */
void task_switch() {
    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_get_next_runnable_task(previous_task);
    tasking_goto_task(next_task);
}

int getpid() {
    if (!_current_task_small) {
        return -1;
    }
    return _current_task_small->id;
}

bool tasking_is_active() {
    return _current_task_small != 0;
}

static void tasking_timer_tick() {
    if (time() >= _current_task_small->current_timeslice_end_date) {
        task_switch();
    }
}

void tasking_unblock_task(task_small_t* task, bool run_immediately) {
    task->blocked_info.status = RUNNABLE;
    if (run_immediately) {
        tasking_goto_task(task);
    }
}

static void tasking_update_blocked_tasks() {
    while (1) {
        task_small_t* task = _task_list_head;
        while (task) {
            if (task->blocked_info.status == RUNNABLE) {
                task = task->next;
                continue;
            }
            else if (task->blocked_info.status == PIT_WAIT) {
                if (time() > task->blocked_info.wake_timestamp) {
                    tasking_unblock_task(task, false);
                }
            }
            else {
                panic("unknown block reason");
            }
            task = task->next;
        }
        sys_yield(RUNNABLE);
    }
}

void tasking_init() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }
    kernel_begin_critical();

    pit_callback = timer_callback_register((void*)tasking_timer_tick, 5, true, 0);
    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = thread_spawn(NULL);
    _task_list_head = _current_task_small;

    thread_spawn((uint32_t)task_sleepy);
    thread_spawn((uint32_t)tasking_update_blocked_tasks);

    printf_info("Multitasking initialized");
    kernel_end_critical();
}
