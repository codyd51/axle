#include "task_small.h"

#include <std/timer.h>
#include <kernel/boot_info.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

#include <kernel/util/amc/amc.h>    // For reaper
#include <kernel/pmm/pmm.h>

#include "mlfq.h"

#define TASK_QUANTUM 50
#define MAX_TASKS 1024

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

void sleep(uint32_t ms) {
    Deprecated();
}

static task_small_t* _tasking_get_next_task(task_small_t* previous_task) {
    Deprecated();
    return NULL;
}

static task_small_t* _tasking_get_next_runnable_task(task_small_t* previous_task) {
    Deprecated();
    return NULL;
}

static task_small_t* _tasking_find_highest_priority_runnable_task(void) {
    Deprecated();
    return NULL;
}

static task_small_t* _tasking_find_unblocked_driver_task(void) {
    task_small_t* iter = _task_list_head;
    while (true) {
        if (iter->blocked_info.status == RUNNABLE && iter->priority == PRIORITY_DRIVER) {
            return iter;
        }
        if (iter->next == NULL) {
            break;
        }
        iter = iter->next;
    }
    return NULL;
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

/*
static void _task_bootstrap(uintptr_t entry_point_ptr, uintptr_t entry_point_arg1, uintptr_t entry_point_arg2, uintptr_t entry_point_arg3) {
    int(*entry_point)(uintptr_t, uintptr_t, uintptr_t) = (int(*)(uintptr_t, uintptr_t, uintptr_t))entry_point_ptr;
    int status = entry_point(entry_point_arg1, entry_point_arg2, entry_point_arg3);
    task_die(status);
}
*/

void _thread_destroy(task_small_t* thread) {
    _task_remove_from_scheduler(thread);

    // Free kernel stack
    kfree(thread->kernel_stack_malloc_head);

    if (!thread->is_thread) {
        // Free AMC service if there is one
        amc_teardown_service_for_task(thread);

        // Free virtual memory space
        vas_state_t* vas_state = thread->vas_state;
        if (vas_state->max_range_count > 255) {
            // TODO(PT): Multi-page VAS
            NotImplemented();
        }
        pml4e_t* pml4 = (pml4e_t*)(PMA_TO_VMA(vas_state->pml4_phys));
        // High memory PDPTs are shared between every process, so no need to touch those
        for (int pml4_iter = 0; pml4_iter < 256; pml4_iter++) {
            if (pml4[pml4_iter].present) {
                uintptr_t pdpt_addr = pml4[pml4_iter].page_dir_pointer_base * PAGE_SIZE;
                printf("Free PDPT 0x%p\n", pdpt_addr);
                pdpe_t* pdpt = (pdpe_t*)(PMA_TO_VMA(pdpt_addr));

                for (int pdpt_iter = 0; pdpt_iter < 512; pdpt_iter++) {
                    if (pdpt[pdpt_iter].present) {
                        uintptr_t page_dir_addr = pdpt[pdpt_iter].page_dir_base * PAGE_SIZE;
                        printf("Free page directory 0x%p\n", page_dir_addr);
                        pde_t* page_dir = (pde_t*)(PMA_TO_VMA(page_dir_addr));

                        for (int page_dir_iter = 0; page_dir_iter < 512; page_dir_iter++) {
                            if (page_dir[page_dir_iter].present) {
                                uintptr_t page_table_addr = page_dir[page_dir_iter].page_table_base * PAGE_SIZE;
                                printf("Free page table 0x%p\n", page_table_addr);
                                pte_t* page_table = (pte_t*)(PMA_TO_VMA(page_table_addr));

                                int freed_page_count = 0;
                                for (int page_table_iter = 0; page_table_iter < 512; page_table_iter++) {
                                    if (page_table[page_table_iter].present) {
                                        uintptr_t page_addr = page_table[page_table_iter].page_base * PAGE_SIZE;
                                        //printf("Free page 0x%p\n", page_addr);
                                        freed_page_count += 1;
                                        pmm_free(page_addr);
                                    }
                                }
                                uintptr_t page_table_mem = 1024*1024*2;
                                uintptr_t page_dir_mem=page_table_mem*512;
                                uintptr_t pdpt_mem = page_dir_mem*512;
                                uintptr_t addr = (page_dir_iter * page_table_mem) + (pdpt_iter * page_dir_mem) + (pml4_iter * pdpt_mem);
                                printf("Freed %d pages at 0x%p\n", freed_page_count, addr);

                                pmm_free(page_table_addr);
                            }
                        }

                        pmm_free(page_dir_addr);
                    }
                }

                pmm_free(pdpt_addr);
            }
        }
        kfree(vas_state);
        /*
        // Free virtual memory space
        uint32_t page_dir_base = (uint32_t)thread->vmm;
        vmm_page_directory_t* virt_page_dir = vas_active_map_phys_range(page_dir_base, sizeof(vmm_page_directory_t));

        // Free AMC service if there is one
        amc_teardown_service_for_task(thread);

        // Free the special page table used to store an allocation bitmap
        uint32_t allocation_state_table_with_flags = virt_page_dir->table_pointers[1022];
        uint32_t allocation_state_table_phys = (allocation_state_table_with_flags) & PAGING_FRAME_MASK;
        pmm_free(allocation_state_table_phys);

        // Free page tables allocated to this program
        // TODO(PT): Anything to think about with shared page tables..?
        // Revisit me when tearing down shared framebuffers / other shared memory testing
        // Iterate all the pages except for the recursive mapping, and the the allocation-state/temp-zone mapping
        vmm_page_directory_t* kernel_vmm_pd = boot_info_get()->vmm_kernel;
        uint32_t freed_page_table_count = 0;
        for (uint32_t i = 0; i < 1024 - 2; i++) {
            uint32_t kernel_page_table_with_flags = kernel_vmm_pd->table_pointers[i];
            uint32_t* kernel_page_table_ptr = kernel_page_table_with_flags & PAGE_DIRECTORY_ENTRY_MASK;
            uint32_t kernel_page_table_flags = (uint32_t)kernel_page_table_with_flags & PAGE_TABLE_FLAG_BITS_MASK;

            uint32_t table_with_flags = virt_page_dir->table_pointers[i];
            uint32_t table_phys = (table_with_flags) & PAGING_FRAME_MASK;
            // x86_64
            //if (!(kernel_page_table_flags & PAGE_PRESENT_FLAG) && (table_with_flags & PAGE_PRESENT_FLAG)) {
            if (false) {
                // Load in the page table so we can free each of its frames
                vmm_page_table_t* table = vas_active_map_temp(table_phys, PAGING_FRAME_SIZE);
                for (uint32_t j = 0; j < 1024; j++) {
                    if (table->pages[j].present) {
                        uint32_t frame_addr = table->pages[j].frame_idx * PAGING_FRAME_SIZE;

                        // Don't free non-general-purpose memory that might've been mapped
                        // into the dead process
                        // (For example, awm has the memory-mapped framebuffer mapped into its VMM)
                        if (pmm_is_frame_general_purpose(frame_addr)) {
                            //printf("Free page %d within table %d (0x%08x)\n", j, i, frame_addr);
                            pmm_free(frame_addr);
                        }
                    }
                }
                vas_active_unmap_temp(sizeof(vmm_page_table_t));

                freed_page_table_count += 1;
                pmm_free(table_phys);
            }
        }

        vmm_unmap_range(vmm_active_pdir(), virt_page_dir, sizeof(vmm_page_directory_t));

        // Free every page constituting the page directory
        for (uint32_t i = 0; i < sizeof(vmm_page_directory_t); i += PAGING_FRAME_SIZE) {
            uint32_t frame_addr = page_dir_base + i;
            pmm_free(frame_addr);
        }
        */
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
    char* stack = kmalloc(stack_size);
    printf("New thread [%d]: Made kernel stack 0x%08x\n", new_task->id, stack);
    memset(stack, 0, stack_size);

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
    printf("\tSet new task's VAS state to 0x%p\n", new_task->vas_state);
    new_task->priority = PRIORITY_NONE;
    new_task->priority_lock.name = "[Task priority spinlock]";

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

static task_small_t* _task_spawn__entry_point_with_args(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, const char* task_name) {
    // Use the internal thread-state constructor so that this task won't get
    // scheduled until we've had a chance to set all of its state
    task_small_t* new_task = _thread_create(entry_point, arg1, arg2, arg3);
    new_task->is_thread = false;

    // By definition, a task is identical to a thread except it has its own VAS
    // The new task's address space is a clone of the task that spawned it
    vas_state_t* new_vas = vas_clone(vas_get_active_state());
    printf("task_spawn set vmm to 0x%p\n", new_vas);
    new_task->vas_state = new_vas;
    task_set_name(new_task, task_name);

    return new_task;
}

static task_small_t* _task_spawn(void* entry_point, task_priority_t priority, const char* task_name) {
    return _task_spawn__entry_point_with_args(entry_point, 0, 0, 0, task_name);
}

static void _task_make_schedulable(task_small_t* task) {
    mlfq_add_task_to_queue(task, 0);
}

static void _task_remove_from_scheduler(task_small_t* task) {
    mlfq_delete_task(task);
}

task_small_t* task_spawn__with_args(void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, const char* task_name) {
    task_small_t* task = _task_spawn__entry_point_with_args(entry_point, arg1, arg2, arg3, task_name);
    // Task is now ready to run - make it schedulable
    _task_make_schedulable(task);
    return task;
}

task_small_t* task_spawn(void* entry_point, task_priority_t priority, const char* task_name) {
    task_small_t* task = _task_spawn(entry_point, priority, task_name);
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
    printf("tasking_first_context_switch 0x%p 0x%p\n", new_task->vas_state, vas_get_active_state());
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

void task_switch_if_driver_ready(void) {
    if (_task_schedule_disabled || !tasking_is_active()) {
        printf("[Schedule] Skipping task-switch because scheduler is disabled\n");
        return;
    }

    // Is any PRIORITY_DRIVER task ready?
    task_small_t* unblocked_driver = _tasking_find_unblocked_driver_task();
    if (unblocked_driver && _current_task_small != unblocked_driver) {
        printk("[%d] Found unblocked driver [%d]\n", getpid(), unblocked_driver->id);
        mlfq_goto_task(unblocked_driver);
    }
    // Continue with the currently running task
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

task_priority_t get_current_task_priority() {
    if (!_current_task_small) {
        return -1;
    }
    return _current_task_small->priority;
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

void tasking_unblock_task_with_reason(task_small_t* task, bool run_immediately, task_state_t reason) {
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
    spinlock_acquire(&task->priority_lock);
    task->blocked_info.unblock_reason = reason;
    task->blocked_info.status = RUNNABLE;
    spinlock_release(&task->priority_lock);
    if (run_immediately) {
        Deprecated();
        //tasking_goto_task(task);
    }
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

void update_blocked_tasks() {
    Deprecated();
}

void iosentinel_check_now() { 
    Deprecated();
}

void idle_task() {
    while (1) {
        asm("sti");
        asm("hlt");
        //sys_yield(RUNNABLE);
    }
}

void reaper_task() {
    amc_register_service("com.axle.reaper");
    spinlock_t reaper_lock = {0};
    reaper_lock.name = "[reaper lock]";
    printf("Reaper running!\n");
    vas_state_dump(vas_get_active_state());

    while (1) {
        amc_message_t* msg;
        amc_message_await_any(&msg);
        printf("Reaper received message!\n");
        vas_state_dump(vas_get_active_state());
        if (strncmp(msg->source, AXLE_CORE_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
            printf("Reaper ignoring message from [%s]\n", msg->source);
            continue;
        }
        task_small_t** buf = (task_small_t**)msg->body;
        task_small_t* zombie_task = buf[0];
        printf("Reaper received corpse [%d %s]\n", zombie_task->id, zombie_task->name);

        spinlock_acquire(&reaper_lock);

        task_small_t* iter = _task_list_head;
        task_small_t* prev = NULL;
        for (int i = 0; i < MAX_TASKS; i++) {
            if (iter == NULL) {
                assert(false, "Reaper failed to find provided task");
                return;
            }

            task_small_t* next = iter->next;
            if (iter == zombie_task) {
                assert(iter->blocked_info.status == ZOMBIE, "Status was not zombie");
                _thread_destroy(iter);

                // Remove this node from the linked-list of tasks
                if (prev != NULL) {
                    prev->next = next;
                }
                else {
                    _task_list_head = next;
                }
                iter = next;
                break;
            }
            prev = iter;
            iter = (iter)->next;
        }

        spinlock_release(&reaper_lock);
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
			printf("Free bootloader PML4E 0x%p\n", pml4e_phys);
			pmm_free(pml4e_phys);
            kernel_pml4[i].present = false;
		}
	}

    printf("tasking_init_part2 continue_func 0x%p\n", continue_func_ptr);
    // idle should not be in the scheduler pool as we schedule it specially
    // _task_spawn will not add it to the scheduler pool
    _idle_task = _task_spawn(idle_task, PRIORITY_IDLE, "idle");

    // reaper cleans up and frees the resources of ZOMBIE tasks
    task_spawn(reaper_task, PRIORITY_NONE, "reaper");

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

    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = thread_spawn(tasking_init_part2, continue_func, 0, 0);
    task_set_name(_current_task_small, "bootstrap");
    _task_list_head = _current_task_small;
    tasking_first_context_switch(_current_task_small, 100);
}

int fork() {
    Deprecated();
    return -1;
}

void* unsbrk(int UNUSED(increment)) {
    NotImplemented();
    return NULL;
}

void* sbrk(int increment) {
	task_small_t* current = tasking_get_current_task();
	printf("[%d] sbrk 0x%p (%u) 0x%p -> 0x%p (current page head 0x%p)\n", getpid(), increment, increment, current->sbrk_current_break, current->sbrk_current_break + increment, current->sbrk_current_page_head);

	if (increment < 0) {
        printf("Relinquish sbrk memory 0x%08x\n", -(uint32_t)increment);
        current->sbrk_current_break -= increment;
		return NULL;
	}

	char* brk = (char*)current->sbrk_current_break;

	if (increment == 0) {
		return brk;
	}

    /*
    while (current->sbrk_current_break + increment >= current->sbrk_current_page_head) {
        uint32_t next_page = current->sbrk_current_page_head;
        current->sbrk_current_page_head += PAGE_SIZE;
        printf("calling vas_alloc_range()\n");
        uint64_t addr = vas_alloc_range(vas_get_active_state(), next_page, PAGE_SIZE, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
        printf("\tgot 0x%p\n", addr);
        if (addr != next_page) {
            printf("sbrk failed to allocate requested page 0x%p\n", addr);
        }
    }
    */
    int64_t needed_pages = ((int64_t)(current->sbrk_current_break + increment) - (int64_t)current->sbrk_current_page_head) / PAGE_SIZE;
    if (needed_pages > 0) {
        printf("\tNeed %d pages (%p - %p = %p)\n", needed_pages, current->sbrk_current_break + increment, current->sbrk_current_page_head,  (current->sbrk_current_break + increment) - current->sbrk_current_page_head);
        //uint64_t page_padded_increment = (increment + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
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

int brk(void* addr) {
    NotImplemented();
    return 0;
}

task_small_t* get_first_responder() {
    Deprecated();
    return NULL;
}

void become_first_responder_pid(int pid) {
    Deprecated();
}

void become_first_responder() {
    become_first_responder_pid(getpid());
}

void resign_first_responder() {
    Deprecated();
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
