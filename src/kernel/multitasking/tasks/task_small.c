#include "task_small.h"

#include <std/timer.h>
#include <kernel/boot_info.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

#define TASK_QUANTUM 10
#define MAX_TASKS 1024

static lock_t* mutex = 0;

static volatile int next_pid = 0;

task_small_t* _current_task_small = 0;
static task_small_t* _task_list_head = 0;

static timer_callback_t* pit_callback = 0;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);

// defined in process_small.s
// performs the actual context switch
void context_switch(uint32_t* new_task);

void task_new() {
    //printf("task_new running\n");
    sleep(5000);
    uint32_t* addr = 0x99900000;
    vmm_page_directory_t* vmm_dir = vmm_active_pdir();
    vmm_alloc_page_address(vmm_dir, addr, true);
    *addr = 0xdeadbeef;
    sleep(500);
    printf("PID %d: 0x%08x: 0x%08x\n", getpid(), addr, *addr);
    while (1) {}
}

void task_sleepy() {
    uint32_t* addr = 0x99900000;
    vmm_page_directory_t* vmm_dir = vmm_active_pdir();
    vmm_alloc_page_address(vmm_dir, addr, true);
    *addr = 0xdeadbeef;
    sleep(500);
    printf("PID %d: 0x%08x: 0x%08x\n", getpid(), addr, *addr);
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
        //printf("next_runnable_task = %d\n", iter->id);
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

void task_die(int exit_code) {
    printf("[%d] self-terminated with exit %d. Zombie\n", getpid(), exit_code);
    // TODO(PT): Clean up the resources associated with the task
    // VMM, stack, file pointers, etc
    tasking_get_current_task()->blocked_info.status = ZOMBIE;
    task_switch();
    panic("Should never be scheduled again\n");
    //sys_yield(ZOMBIE);
}

static void _task_bootstrap(uint32_t entry_point_ptr, uint32_t arg2) {
    int(*entry_point)(void) = (int(*)(void))entry_point_ptr;
    int status = entry_point();
    task_die(status);
}

task_small_t* _thread_create(void* entry_point) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;
    new_task->blocked_info.status = RUNNABLE;

    uint32_t stack_size = 0x2000;
    char *stack = kmalloc(stack_size);
    memset(stack, 0, stack_size);

    uint32_t* stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack
    if (entry_point) {
        //printf_info("thread_create ent 0x%08x", entry_point);
        *(stack_top--) = entry_point;   // Argument to bootstrap function (which we'll then jump to)
        *(stack_top--) = 0;     // Alignment
        *(stack_top--) = _task_bootstrap;   // Entry point for new thread
        *(stack_top--) = 0;             //eax
        *(stack_top--) = 0;             //ebx
        *(stack_top--) = 0;             //esi
        *(stack_top--) = 0;             //edi
        *(stack_top)   = 0;             //ebp
    }

    new_task->machine_state = (task_context_t*)stack_top;

    new_task->is_thread = true;
    new_task->vmm = vmm_active_pdir();
}

task_small_t* thread_spawn(void* entry_point) {
    task_small_t* new_thread = _thread_create(entry_point);
    // Make the thread schedulable now
    _tasking_add_task_to_runlist(new_thread);
    return new_thread;
}

task_small_t* task_spawn(void* entry_point) {
    lock(mutex);
    // Use the internal thread-state constructor so that this task won't get
    // scheduled until we've had a chance to set all of its state
    task_small_t* new_task = _thread_create(entry_point);
    new_task->is_thread = false;
    // a task is simply a thread with its own virtual address space
    // the new task's address space is a clone of the task that spawned it
    vmm_page_directory_t* new_vmm = vmm_clone_active_pdir();
    new_task->vmm = new_vmm;
    // Task is now ready to run - make it schedulable
    _tasking_add_task_to_runlist(new_task);

    unlock(mutex);
    return new_task;
}

/*
 * Immediately preempt the running task and begin running the provided one.
 */
void tasking_goto_task(task_small_t* new_task) {
    kernel_begin_critical();
    lock(mutex);
    //assert(new_task != _current_task_small, "new_task == _current task");
    uint32_t now = time();
    new_task->current_timeslice_start_date = now;
    new_task->current_timeslice_end_date = now + TASK_QUANTUM;

    // Ensure that any shared page tables between the kernel and the preempted VMM have an in-sync allocation state
    // This check should no longer be needed, since allocations within the shared kernel pages are always
    // marked within the shared kernel bitmap. 
    // However, keep the check in to ensure this never regresses.
    vmm_validate_shared_tables_in_sync(vmm_active_pdir(), boot_info_get()->vmm_kernel);

    if (new_task->vmm != vmm_active_pdir()) {
        vmm_load_pdir(new_task->vmm, false);
    }

    // this method will update _current_task_small
    // this method performs the actual context switch and also updates _current_task_small
    unlock(mutex);
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
    return _current_task_small != 0 && pit_callback != 0;
}

static void tasking_timer_tick() {
    kernel_begin_critical();
    if (time() > _current_task_small->current_timeslice_end_date) {
        task_switch();
    }
    else {
        kernel_end_critical();
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

void idle_task() {
    while (1) {
        asm("hlt");
    }
}

void tasking_init() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }

	mutex = lock_create();

    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = thread_spawn(NULL);
    //strncpy(_current_task_small->name, "bootstrap", 10);
    _task_list_head = _current_task_small;

    /*
    task_small_t* t = task_spawn((uint32_t)task_sleepy);
    printf("bootstrap task is pid %d\n", _current_task_small->id);
    printf("sleepy task is pid %d\n", t->id);
    task_spawn((uint32_t)task_new);
    thread_spawn((uint32_t)tasking_update_blocked_tasks);
    */
    task_spawn(idle_task);

    printf_info("Multitasking initialized");

    pit_callback = timer_callback_register((void*)tasking_timer_tick, 100, true, 0);
}

int fork() {
    Deprecated();
}

void* unsbrk(int UNUSED(increment)) {
    NotImplemented();
    return NULL;
}

void* sbrk(int increment) {
	//printf("sbrk 0x%08x\n", increment);
	if (increment < 0) {
		ASSERT(0, "sbrk w/ neg increment");
		return NULL;
	}

	task_small_t* current = tasking_get_current_task();
	char* brk = (char*)current->sbrk_current_break;

	if (increment == 0) {
		return brk;
	}

	current->sbrk_current_break += increment;
    if (current->sbrk_current_break > current->bss_segment_addr + PAGING_PAGE_SIZE) {
        // Not implemented yet
        panic("Need to expand sbrk region by allocating more pages");
    }

	memset(brk, 0, increment);
	return brk;
}

int brk(void* addr) {
    NotImplemented();
    return 0;
}