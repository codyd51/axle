#include "task_small.h"

#include <std/timer.h>
#include <kernel/boot_info.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

#include <kernel/pmm/pmm.h>

#include "mlfq.h"
#include "task_small_int.h"
#include "reaper.h"

static volatile int next_pid = 0;

task_small_t* _current_task_small = 0;
static task_small_t* _current_first_responder = 0;
static task_small_t* _iosentinel_task = 0;
static task_small_t* _task_list_head = 0;
static task_small_t* _idle_task = 0;

static bool _multitasking_ready = false;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);

// defined in process_small.s
// performs the actual context switch
void context_switch(uintptr_t* new_task);
void _first_context_switch(uintptr_t* new_task);
// Defined in process_small.s
// Entry point for a new process
void _task_bootstrap(uintptr_t entry_point_ptr, uintptr_t entry_point_arg1, uintptr_t entry_point_arg2, uintptr_t entry_point_arg3);

static void _task_make_schedulable(task_small_t* task);
static void _task_remove_from_scheduler(task_small_t* task);

task_small_t* _tasking_get_linked_list_head(void) {
    return _task_list_head;
}

void _tasking_set_linked_list_head(task_small_t* new_head) {
    _task_list_head = new_head;
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
    Deprecated();
    return NULL;
}

static void _tasking_add_task_to_task_list(task_small_t* task) {
    if (!_current_task_small) {
        _current_task_small = task;
        return;
    }
    task_small_t* list_tail = _tasking_last_task_in_runlist();
    list_tail->next = task;
}

task_small_t* tasking_get_task_with_pid(int pid) {
    if (!_task_list_head) {
        return NULL;
    }
    task_small_t* iter = _task_list_head;
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
    task_small_t* buf[1] = {tasking_get_current_task()};
    amc_message_send__from_core("com.axle.reaper", &buf, sizeof(buf));
    // Set ourselves to zombie _after_ telling reaper about us
    // Even if we're pre-empted in between switching to zombie and informing reaper,
    // we'll still be cleaned up
    tasking_get_current_task()->blocked_info.status = ZOMBIE;
    task_switch();
    panic("Should never be scheduled again\n");
}

void _thread_destroy(task_small_t* thread) {
    _task_remove_from_scheduler(thread);

    // Free kernel stack
    //printf("Free kernel stack 0x%p\n", thread->kernel_stack_malloc_head);
    kfree(thread->kernel_stack_malloc_head);

    if (!thread->is_thread) {
        // Free AMC service if there is one
        amc_teardown_service_for_task(thread);

        // Free virtual memory space
        vas_teardown(thread->vas_state);
    }
    else {
        printf("\tWill not free VMM because this is a thread\n");
    }

    // Free control block
    kfree(thread->name);
    kfree(thread);
}

void task_set_name(task_small_t* task, const char* new_name) {
    if (task->name) {
        kfree(task->name);
    }
    task->name = strdup(new_name);
}

task_small_t* _thread_create(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;
    new_task->blocked_info.status = RUNNABLE;

    uint32_t stack_size = 0x2000;
    char* stack = kcalloc(1, stack_size);
    //printf("New thread [%d]: Made kernel stack 0x%08x\n", new_task->id, stack);

    uintptr_t* stack_top = (uintptr_t*)(stack + stack_size - sizeof(uintptr_t)); // point to top of malloc'd stack
    if (entry_point) {
        *(stack_top--) = arg3;
        *(stack_top--) = arg2;
        *(stack_top--) = arg1;
        *(stack_top--) = (uintptr_t)entry_point;   // Argument to bootstrap function (which we'll then jump to)
        *(stack_top--) = 0x0;   // Alignment
        *(stack_top--) = (uintptr_t)_task_bootstrap;   // Entry point for new thread
        *(stack_top--) = 0;             //eax
        *(stack_top--) = 0;             //ebx
        *(stack_top--) = 0;             //esi
        *(stack_top--) = 0;             //edi
        *(stack_top)   = 0;             //ebp
    }

    new_task->machine_state = (task_context_t*)stack_top;
    new_task->kernel_stack = stack_top;
    new_task->kernel_stack_malloc_head = stack;

    new_task->is_thread = true;
    new_task->vas_state = vas_get_active_state();
    //printf("\tSet new task's VAS state to 0x%p\n", new_task->vas_state);

    // Retain a reference to this task in the linked list of all tasks
    _tasking_add_task_to_task_list(new_task);

    return new_task;
}

task_small_t* thread_spawn(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* new_thread = _thread_create(entry_point, arg1, arg2, arg3);
    // Make the thread schedulable now
    _task_make_schedulable(new_thread);
    return new_thread;
}

static task_small_t* _task_spawn__entry_point_with_args(const char* task_name, void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    // Use the internal thread-state constructor so that this task won't get
    // scheduled until we've had a chance to set all of its state
    task_small_t* new_task = _thread_create(entry_point, arg1, arg2, arg3);
    new_task->is_thread = false;

    // By definition, a task is identical to a thread except it has its own VAS
    // The new task's address space is a clone of the task that spawned it
    vas_state_t* new_vas = vas_clone(vas_get_active_state());
    new_task->vas_state = new_vas;
    task_set_name(new_task, task_name);

    return new_task;
}

static task_small_t* _task_spawn(const char* task_name, void* entry_point) {
    return _task_spawn__entry_point_with_args(task_name, entry_point, 0, 0, 0);
}

static void _task_make_schedulable(task_small_t* task) {
    mlfq_add_task_to_queue(task, 0);
}

static void _task_remove_from_scheduler(task_small_t* task) {
    mlfq_delete_task(task);
}

task_small_t* task_spawn__with_args(const char* task_name, void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* task = _task_spawn__entry_point_with_args(task_name, entry_point, arg1, arg2, arg3);
    // Task is now ready to run - make it schedulable
    _task_make_schedulable(task);
    return task;
}

task_small_t* task_spawn(const char* task_name, void* entry_point) {
    task_small_t* task = _task_spawn(task_name, entry_point);
    // Task is now ready to run - make it schedulable
    _task_make_schedulable(task);
    return task;
}

/*
 * Immediately preempt the running task and begin running the provided one.
 */
void tasking_goto_task(task_small_t* new_task, uint32_t quantum) {
    uint32_t now = ms_since_boot();
    new_task->current_timeslice_start_date = now;
    new_task->current_timeslice_end_date = now + quantum;

    //printf("tasking_goto_task new vmm 0x%p current vmm 0x%p\n", new_task->vas_state, vas_get_active_state());
    //printf("tasking_goto_task [%s] from [%s]\n", new_task->name, _current_task_small->name);
    if (new_task->vas_state != vas_get_active_state()) {
        //printf("\tLoad new VAS state 0x%p\n", new_task->vas_state);
        vas_load_state(new_task->vas_state);
    }

    //printf("\tSet kernel stack to 0x%p\n", new_task->kernel_stack);
    tss_set_kernel_stack(new_task->kernel_stack);
    // this method will update _current_task_small
    // this method performs the actual context switch and also updates _current_task_small
    context_switch(new_task);
}

void tasking_first_context_switch(task_small_t* new_task, uint32_t quantum) {
    uint32_t now = ms_since_boot();
    new_task->current_timeslice_start_date = now;
    new_task->current_timeslice_end_date = now + quantum;

    // TODO(PT): Needed?
    //printf("tasking_first_context_switch 0x%p 0x%p\n", new_task->vas_state, vas_get_active_state());
    if (new_task->vas_state != vas_get_active_state()) {
        vas_load_state(new_task->vas_state);
    }

    //tss_set_kernel_stack(new_task->kernel_stack);
    _first_context_switch(new_task);
}

static bool _task_schedule_disabled = false;

void tasking_disable_scheduling(void) {
    _task_schedule_disabled = true;
}

void tasking_reenable_scheduling(void) {
    _task_schedule_disabled = false;
}

/*
 * Pick the next task to schedule, and preempt the currently running one.
 */

void task_switch(void) {
    asm("cli");
    if (_task_schedule_disabled) {
        printf("[Schedule] Skipping task-switch because scheduler is disabled\n");
        return;
    }

    // Tell the scheduler about the task switch
    mlfq_prepare_for_switch_from_task(_current_task_small);
    task_small_t* next_task = 0;
    uint32_t quantum = 0;
    mlfq_choose_task(&next_task, &quantum);
    if (!next_task) {
        // Fallback to the idle task if nothing else is ready to run
        //printf("Fallback to idle task\n");
        //mlfq_print();
        next_task = _idle_task;
        quantum = 5;
    }

    //if (next_task != _current_task_small) {
        //printf("Schedule [%d %s] for %d\n", next_task->id, next_task->name, quantum);
        tasking_goto_task(next_task, quantum);
    //}
}

void mlfq_goto_task(task_small_t* task) {
    if (_current_task_small == task) return;

    mlfq_prepare_for_switch_from_task(_current_task_small);
    uint32_t quantum = 0;
    mlfq_next_quantum_for_task(task, &quantum);
    tasking_goto_task(task, quantum);
}

void task_switch_if_quantum_expired(void) {
    //printf("_current_task_small->current_timeslice_end_date %p %d\n", _current_task_small, _current_task_small->current_timeslice_end_date);
    if (_task_schedule_disabled || !tasking_is_active()) {
        return;
    }

    mlfq_priority_boost_if_necessary();

    if (ms_since_boot() >= _current_task_small->current_timeslice_end_date) {
        //asm("sti");
        //printf("[%d] quantum expired at %d, %d\n", getpid(), ms_since_boot(), _current_task_small->current_timeslice_end_date);
        task_switch();
    }
    //else {
    //    printf("[%d] quantum not expired (%dms remaining)\n", _current_task_small->id, _current_task_small->current_timeslice_end_date - ms_since_boot());
    //}
}

int getpid() {
    if (!_current_task_small) {
        return -1;
    }
    return _current_task_small->id;
}

bool tasking_is_active() {
    return _current_task_small != 0 && _multitasking_ready == true;
}

static void tasking_timer_tick() {
    Deprecated();
    //kernel_begin_critical();
    if (ms_since_boot() > _current_task_small->current_timeslice_end_date) {
        task_switch();
    }
}

void tasking_unblock_task_with_reason(task_small_t* task, task_state_t reason) {
    // Is this a reason why we're blocked?
    /*
    if (!(task->blocked_info.status & reason)) {
        printf("tasking_unblock_task_with_reason(%s, %d) called with reason the task is not blocked for (%d)\n", task->name, reason, task->blocked_info.status);
        assert(0, "invalid call to tasking_unblock_task_with_reason");
        return;
    }
    */
    if (task == _current_task_small) {
        // One reason this code path gets hit is an interrupt is received
        // while the driver is processing the previous interrupt
        return;
    }
    // Record why we unblocked
    //spinlock_acquire(&task->priority_lock);
    task->blocked_info.unblock_reason = reason;
    task->blocked_info.status = RUNNABLE;
    //spinlock_release(&task->priority_lock);
}

void tasking_block_task(task_small_t* task, task_state_t blocked_state) {
    // Some states are invalid "blocked" states
    if (blocked_state == RUNNABLE || blocked_state == ZOMBIE) {
        panic("Invalid blocked state");
    }
    task->blocked_info.status = blocked_state;
    // If the current task just became blocked, switch to another
    if (task == _current_task_small) {
        //printf("Switch due to blocked task\n");
        task_switch();
    }
}

void idle_task() {
    while (1) {
        asm("sti");
        asm("hlt");
    }
}

void tasking_init_part2(void* continue_func_ptr) {
    // We're now fully established in high memory and using a high kernel stack
    // It's now safe to free the low-memory identity map
    vas_state_t* kernel_vas = boot_info_get()->vas_kernel;
    vas_range_t* low_identity_map_range = NULL;
    for (int i = 0; i < kernel_vas->range_count; i++) {
        vas_range_t* range = &kernel_vas->ranges[i];
        if (range->start == 0x0) {
            low_identity_map_range = range;
            break;
        }
    }
    assert(low_identity_map_range, "Failed to find low-memory identity map");
    vas_delete_range(kernel_vas, low_identity_map_range->start, low_identity_map_range->size);
    // Free the low PML4 entries
    // These all use 1GB pages, so we only need to free the PML4E's themselves,
    // and not any lower-level paging structures
    pml4e_t* kernel_pml4 = (pml4e_t*)PMA_TO_VMA(kernel_vas->pml4_phys);
	for (int i = 0; i < 256; i++) {
		if (kernel_pml4[i].present) {
			uint64_t pml4e_phys = kernel_pml4[i].page_dir_pointer_base * PAGE_SIZE;
			//printf("Free bootloader PML4E 0x%p\n", pml4e_phys);
			pmm_free(pml4e_phys);
            kernel_pml4[i].present = false;
		}
	}

    //printf("tasking_init_part2 continue_func 0x%p\n", continue_func_ptr);
    // idle should not be in the scheduler pool as we schedule it specially
    // _task_spawn will not add it to the scheduler pool
    _idle_task = _task_spawn("idle", idle_task);

    // reaper cleans up and frees the resources of ZOMBIE tasks
    task_spawn("reaper", reaper_task);

    printf_info("Multitasking initialized");
    _multitasking_ready = true;
    asm("sti");

    // Wait until reaper wakes up so it can reliably kill every service
    while (!amc_service_is_active("com.axle.reaper")) {
        asm("hlt");
    }

    void(*continue_func)(void) = (void(*)(void))continue_func_ptr;
    continue_func();
}

void tasking_init(void* continue_func) {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }

    mlfq_init();

    _current_task_small = thread_spawn(tasking_init_part2, continue_func, 0, 0);
    task_set_name(_current_task_small, "bootstrap");
    _task_list_head = _current_task_small;
    tasking_first_context_switch(_current_task_small, 100);
}

void* sbrk(int increment) {
	task_small_t* current = tasking_get_current_task();
	//printf("[%d] sbrk 0x%p (%u) 0x%p -> 0x%p (current page head 0x%p)\n", getpid(), increment, increment, current->sbrk_current_break, current->sbrk_current_break + increment, current->sbrk_current_page_head);

	if (increment < 0) {
        printf("Relinquish sbrk memory 0x%08x\n", -(uint32_t)increment);
        current->sbrk_current_break -= increment;
		return NULL;
	}

	char* brk = (char*)current->sbrk_current_break;

	if (increment == 0) {
		return brk;
	}

    int64_t new_high = current->sbrk_current_break + increment;
    if (new_high > current->sbrk_current_page_head) {
        int64_t needed_pages = (new_high - current->sbrk_current_page_head + (PAGE_SIZE - 1)) / PAGE_SIZE;
        //printf("need %d pages, current break %p, incr %p, current head %p, new_hih %d\n", needed_pages, current->sbrk_current_break, increment, current->sbrk_current_page_head, new_high);
        //printf("[%d] sbrk reserve %dkb\n", getpid(), needed_pages * (PAGE_SIZE / 1024));
        uint64_t addr = vas_alloc_range(vas_get_active_state(), current->sbrk_current_page_head, needed_pages * PAGE_SIZE, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
        if (addr != current->sbrk_current_page_head) {
            printf("sbrk failed to allocate requested page 0x%p\n", addr);
        }
        current->sbrk_current_page_head += needed_pages * PAGE_SIZE;
    }
    current->sbrk_current_break += increment;

    // TODO(PT): Just solved a bug where create_shared_memory_region()
    // was allocating pages that otherwise would've been handed out by sbrk
    // and sbrk() didn't panic that the page was already alloc'd because
    // vmm_address_is_mapped() was checked
    // Maybe we pre-reserve a big sbrk area and hand out shared memory regions well above it

	memset(brk, 0, increment);
	return brk;
}

void tasking_print_processes(void) {
    printk("-----------------------proc-----------------------\n");

    if (!_task_list_head) {
        return;
    }
    task_small_t* iter = _task_list_head;
    for (int i = 0; i < MAX_TASKS; i++) {
        printk("[%d] %s ", iter->id, iter->name);
            if (iter == _current_task_small) {
                printk("(active)");
            }

            if (iter == _current_task_small) {
                printk("(active for %d ms more) ", iter->current_timeslice_end_date - ms_since_boot());
            }
            else {
                printk("(inactive since %d ms ago)", ms_since_boot() - iter->current_timeslice_end_date);
            }

            switch (iter->blocked_info.status) {
                case RUNNABLE:
                    printk("(runnable)");
                    break;
                case ZOMBIE:
                    printk("(zombie)");
                    break;
                case KB_WAIT:
                    printk("(blocked by keyboard)");
                    break;
                case AMC_AWAIT_MESSAGE:
                    printk("(await AMC)");
                    break;
                case VMM_MODIFY:
                    printk("(kernel VMM manipulation)");
                    break;
                default:
                    printk("(unknwn)");
                    break;
            }
            printk("\n");

        if ((iter)->next == NULL) {
            break;
        }
        iter = (iter)->next;
    }
    printk("---------------------------------------------------\n");
}
