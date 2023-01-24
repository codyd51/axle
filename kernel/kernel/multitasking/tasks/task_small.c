#include "task_small.h"

#include <std/timer.h>
#include <kernel/boot_info.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>
#include <kernel/util/amc/amc_internal.h>
#include <kernel/util/amc/core_commands.h>
#include <kernel/assert.h>
#include <kernel/smp.h>

#include <kernel/pmm/pmm.h>

#include "mlfq.h"
#include "task_small_int.h"
#include "reaper.h"

static volatile int next_pid = 0;

static task_small_t* _current_first_responder = 0;
static task_small_t* _iosentinel_task = 0;
static task_small_t* _task_list_head = 0;

static bool _multitasking_ready = false;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);

// defined in process_small.s
// performs the actual context switch
void context_switch(uintptr_t* new_task, task_small_t* cpu_current_task_ptr);
void _first_context_switch(uintptr_t* new_task, task_small_t* cpu_current_task_ptr);
// Defined in process_small.s
// Entry point for a new process
void _task_bootstrap(uintptr_t entry_point_ptr, uintptr_t entry_point_arg1, uintptr_t entry_point_arg2, uintptr_t entry_point_arg3);

static void _task_make_schedulable(task_small_t* task);
static void _task_remove_from_scheduler(task_small_t* task);

void ap_spin1(void);
void ap_spin2(void);

task_small_t* _tasking_get_linked_list_head(void) {
    // TODO(PT): Deprecate
    return _task_list_head;
}

void _tasking_set_linked_list_head(task_small_t* new_head) {
    _task_list_head = new_head;
}

static task_small_t* cpu_current_task(void) {
    return cpu_private_info()->current_task;
}

static void cpu_set_current_task(task_small_t* t) {
    cpu_private_info()->current_task = t;
}

static bool cpu_scheduler_enabled(void) {
    return cpu_private_info()->scheduler_enabled;
}

static void cpu_set_scheduler_enabled(bool enabled) {
    cpu_private_info()->scheduler_enabled = enabled;
}

static void cpu_set_idle_task(task_small_t* idle_task) {
    cpu_private_info()->idle_task = idle_task;
}

static task_small_t* _tasking_last_task_in_runlist() {
    if (!cpu_current_task()) {
        return NULL;
    }
    task_small_t* iter = cpu_current_task();
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
    if (!cpu_current_task()) {
        cpu_set_current_task(task);
        return;
    }
    task_small_t* list_tail = _tasking_last_task_in_runlist();
    list_tail->next = task;
}

task_small_t* tasking_get_current_task() {
    return tasking_get_task_with_pid(getpid());
}

void task_die(uintptr_t exit_code) {
    printf("[%d] self-terminated with exit %d (0x%x). Zombie\n", getpid(), exit_code, exit_code);

    task_inform_supervisor__process_exit(exit_code);
    // Inform our supervisor, if any
    task_small_t* current_task = tasking_get_current_task();
    if (current_task->is_managed_by_parent) {
        // TODO(PT): Looks like this may have been accidentally deleted?
    }

    task_small_t* buf[1] = {tasking_get_current_task()};
    amc_message_send__from_core("com.axle.reaper", &buf, sizeof(buf));
    // Set ourselves to zombie _after_ telling reaper about us
    // Even if we're pre-empted in between switching to zombie and informing reaper,
    // we'll still be cleaned up
    tasking_get_current_task()->blocked_info.status = ZOMBIE;
    task_switch();
    panic("Should never be scheduled again");
}

void _thread_destroy(task_small_t* thread) {
    _task_remove_from_scheduler(thread);

    // Free kernel stack
    //printf("Free kernel stack 0x%p\n", thread->kernel_stack_malloc_head);
    kfree((void*)thread->kernel_stack_malloc_head);

    // Free the string table and symbol table that were copied to the heap
    // TODO(PT): These are only heap copies when the underlying program was loaded from an ELF
    if (thread->elf_symbol_table.strtab) {
        kfree((void *) thread->elf_symbol_table.strtab);
    }
    if (thread->elf_symbol_table.symtab) {
        kfree((void *) thread->elf_symbol_table.symtab);
    }

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
    if (thread->is_managed_by_parent) {
        kfree(thread->managing_parent_service_name);
    }
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
    new_task->cpu_id = cpu_id();

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
    //_tasking_add_task_to_task_list(new_task);
    scheduler_track_task(new_task);

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
    new_task->is_managed_by_parent = false;
    new_task->managing_parent_service_name = NULL;

    // By definition, a task is identical to a thread except it has its own VAS
    // The new task's address space is 'fresh', i.e. only contains kernel mappings
    vas_state_t* new_vas = vas_clone(cpu_private_info()->base_vas);
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

spinlock_t* choose_task_lock(void) {
    static spinlock_t choose_task_spinlock = {0};
    choose_task_spinlock.name = "[Sched choose task]";
    return &choose_task_spinlock;
}

task_small_t* task_spawn__with_args(const char* task_name, void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* task = _task_spawn__entry_point_with_args(task_name, entry_point, arg1, arg2, arg3);
    // Task is now ready to run - make it schedulable
    _task_make_schedulable(task);
    return task;
}

task_small_t* task_spawn__managed__with_args(const char* task_name, void* entry_point, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    task_small_t* task = _task_spawn__entry_point_with_args(task_name, entry_point, arg1, arg2, arg3);
    task->is_managed_by_parent = true;
    amc_service_t* parent = amc_service_of_active_task();
    task_assert(parent != NULL, "task_spawn__managed__with_args called without an AMC service", NULL);
    task->managing_parent_service_name = strdup(parent->name);
    // Inform the supervisor of the child's PID
    // Do it before the task becomes schedulable to avoid races on message order
    task_inform_supervisor__process_create__with_task(task, (uint64_t)task->id);
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
    // TODO(PT): Update the TSS from the host CPU
    tss_set_kernel_stack(new_task->kernel_stack);
    // this method will update cpu->current_task
    // this method performs the actual context switch and also updates _current_task_small
    context_switch(new_task, &cpu_private_info()->current_task);
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
    _first_context_switch(new_task, &cpu_private_info()->current_task);
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

    // Cancel the LAPIC timer, if any
    local_apic_timer_cancel();

    // Tell the scheduler about the task switch
    mlfq_prepare_for_switch_from_task(cpu_current_task());
    task_small_t* next_task = 0;
    uint32_t quantum = 0;

    spinlock_acquire(choose_task_lock());
    mlfq_choose_task(&next_task, &quantum);
    spinlock_release(choose_task_lock());

    if (!next_task) {
        // Fallback to the idle task if nothing else is ready to run
        //printf("Fallback to idle task\n");
        //mlfq_print();
        next_task = cpu_idle_task();
        quantum = 5;
    }

    // Set up the scheduler timer
    // But note that we won't be able to do this in very early boot
    if (smp_info_get()) {
        //printf("Arming LAPIC timer for %dms\n", quantum);
        local_apic_timer_start(quantum);
    }

    //if (next_task != _current_task_small) {
        //printf("Schedule [%d %s] for %d\n", next_task->id, next_task->name, quantum);
        tasking_goto_task(next_task, quantum);
    //}
}

void mlfq_goto_task(task_small_t* task) {
    if (cpu_current_task() == task) return;

    mlfq_prepare_for_switch_from_task(cpu_current_task());
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

    if (ms_since_boot() >= cpu_current_task()->current_timeslice_end_date) {
        //asm("sti");
        //printf("[%d] quantum expired at %d, %d\n", getpid(), ms_since_boot(), _current_task_small->current_timeslice_end_date);
        task_switch();
    }
    //else {
    //    printf("[%d] quantum not expired (%dms remaining)\n", _current_task_small->id, _current_task_small->current_timeslice_end_date - ms_since_boot());
    //}
}

int getpid() {
    if (!_multitasking_ready || !cpu_current_task()) {
        return -1;
    }
    return cpu_current_task()->id;
}

bool tasking_is_active() {
    return _multitasking_ready && cpu_current_task() && cpu_scheduler_enabled();
}

static void tasking_timer_tick() {
    Deprecated();
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
    if (task == cpu_current_task()) {
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
    if (task == cpu_current_task()) {
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

static void _free_low_identity_map(vas_state_t* vas) {
    vas_range_t* low_identity_map_range = NULL;
    for (int i = 0; i < vas->range_count; i++) {
        vas_range_t* range = &vas->ranges[i];
        if (range->start == 0x0) {
            low_identity_map_range = range;
            break;
        }
    }
    assert(low_identity_map_range, "Failed to find low-memory identity map");
    vas_delete_range(vas, low_identity_map_range->start, low_identity_map_range->size);
    // Free the low PML4 entries
    // These all use 1GB pages, so we only need to free the PML4E's themselves,
    // and not any lower-level paging structures
    // TODO(PT): Is the above still true? I'm not sure that the bootloader is still using 1GB pages: IIRC
    // they caused issues on my hardware.
    pml4e_t* vas_pml4 = (pml4e_t*)PMA_TO_VMA(vas->pml4_phys);
    // TODO(PT): This should only free the exact range that was identity mapped, rather than all low canonical memory
    for (int i = 0; i < 256; i++) {
        if (vas_pml4[i].present) {
            uint64_t pml4e_phys = vas_pml4[i].page_dir_pointer_base * PAGE_SIZE;
            printf("Free low PML4E #%d: 0x%p\n", i, pml4e_phys);
            pmm_free(pml4e_phys);
            vas_pml4[i].present = false;
        }
    }
}

static void _spawn_cpu_idle_task(void) {
    // idle should not be in the scheduler pool as we schedule it specially
    // _task_spawn will not add it to the scheduler pool
    char idle_task_name[64];
    snprintf(idle_task_name, sizeof(idle_task_name), "com.axle.idle-%d", cpu_id());
    cpu_set_idle_task(_task_spawn(idle_task_name, idle_task));
}

void tasking_ap_init_part2(void* continue_func_ptr) {
    // It's now safe to free the low-memory identity map
    _free_low_identity_map(cpu_private_info()->base_vas);
    _spawn_cpu_idle_task();

    // TODO(PT): This won't do anything because the PIT is currently only delivered to APIC #0. We should start using the APIC-local timer
    cpu_set_scheduler_enabled(true);

    void(*continue_func)(void) = (void(*)(void))continue_func_ptr;
    continue_func();
}

void tasking_init_part2(void* continue_func_ptr) {
    // We're now fully established in high memory and using a high kernel stack
    // It's now safe to free the low-memory identity map
    _free_low_identity_map(cpu_private_info()->base_vas);
    _spawn_cpu_idle_task();

    // reaper cleans up and frees the resources of ZOMBIE tasks
    task_small_t* reaper_tcb = task_spawn("reaper", reaper_task);

    printf("Multitasking initialized\n");
    _multitasking_ready = true;
    cpu_set_scheduler_enabled(true);

    // Context switch to the reaper with a small quantum so it has time to set up its AMC service
    // This way, we're sure that from here reaper is always ready to tear down processes
    tasking_goto_task(reaper_tcb, 5);
    assert(amc_service_is_active("com.axle.reaper"), "Reaper AMC service didn't come up as expected");

    void(*continue_func)(void) = (void(*)(void))continue_func_ptr;
    continue_func();
}

void tasking_init(void* continue_func) {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }

    mlfq_init();

    cpu_set_current_task(thread_spawn(tasking_init_part2, continue_func, 0, 0));
    task_set_name(cpu_current_task(), "bootstrap");
    _task_list_head = cpu_current_task();
    tasking_first_context_switch(cpu_current_task(), 100);
}

void tasking_ap_startup(void* continue_func) {
    // TODO(PT): Free initial AP stack/page tables?
    // Prime the scheduler
    task_small_t* ap_bootstrap_task = thread_spawn(tasking_ap_init_part2, (uintptr_t)continue_func, 0, 0);
    cpu_set_current_task(ap_bootstrap_task);
    task_set_name(ap_bootstrap_task, "ap_bootstrap");
    tasking_first_context_switch(ap_bootstrap_task, 100);
}

void* sbrk(int increment) {
	task_small_t* current = tasking_get_current_task();
	//printf("[%d] sbrk 0x%p (%u) 0x%p -> 0x%p (current page head 0x%p)\n", getpid(), increment, increment, current->sbrk_current_break, current->sbrk_current_break + increment, current->sbrk_current_page_head);

	if (increment < 0) {
        printf("Relinquish sbrk memory 0x%08x\n", -(uint32_t)increment);
        current->sbrk_current_break -= increment;
        assert(current->sbrk_current_break >= current->sbrk_base, "Underflow brk region");
		return NULL;
	}

	char* brk = (char*)current->sbrk_current_break;

	if (increment == 0) {
		return brk;
	}

    int64_t new_high = current->sbrk_current_break + increment;
    if (new_high > current->sbrk_current_page_head) {
        int64_t needed_pages = (new_high - current->sbrk_current_page_head + (PAGE_SIZE - 1)) / PAGE_SIZE;
        //printf("need %d pages, current break %p, incr %p, current head %p, new_high %p\n", needed_pages, current->sbrk_current_break, increment, current->sbrk_current_page_head, new_high);
        //printf("[%d] sbrk reserve %dkb\n", getpid(), needed_pages * (PAGE_SIZE / 1024));
        uint64_t addr = vas_alloc_range(vas_get_active_state(), current->sbrk_current_page_head, needed_pages * PAGE_SIZE, VAS_RANGE_ACCESS_LEVEL_READ_WRITE, VAS_RANGE_PRIVILEGE_LEVEL_USER);
        if (addr != current->sbrk_current_page_head) {
            printf("sbrk failed to allocate requested page 0x%p, current sbrk head 0x%p\n", addr, current->sbrk_current_page_head);
            vas_state_dump(vas_get_active_state());
            assert(false, "sbrk fail");
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
    task_small_t* iter = _tasking_get_linked_list_head();
    for (int i = 0; i < MAX_TASKS; i++) {
        printk("[%d] %s ", iter->id, iter->name);
            if (iter == cpu_current_task()) {
                printk("(active)");
            }

            if (iter == cpu_current_task()) {
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
