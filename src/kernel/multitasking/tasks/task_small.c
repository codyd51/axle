#include "task_small.h"

#include <std/timer.h>
#include <kernel/boot_info.h>
#include <kernel/multitasking/std_stream.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/segmentation/gdt_structures.h>

#define TASK_QUANTUM 20
#define MAX_TASKS 1024

static volatile int next_pid = 0;

task_small_t* _current_task_small = 0;
static task_small_t* _current_first_responder = 0;
static task_small_t* _iosentinel_task = 0;
static task_small_t* _task_list_head = 0;

static timer_callback_t* pit_callback = 0;
const uint32_t _task_context_offset = offsetof(struct task_small, machine_state);

// defined in process_small.s
// performs the actual context switch
void context_switch(uint32_t* new_task);

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
    return NULL;
}

static task_small_t* _tasking_find_highest_priority_runnable_task(void) {
    task_small_t* iter = _task_list_head;
    int32_t highest_runnable_priority = -1;
    task_small_t* highest_priority_runnable_task = NULL;
    while (true) {
        //printf("Check task [%d] %s %d %d\n", iter->id, iter->name, iter->blocked_info.status, iter->priority);

        if (iter->blocked_info.status == RUNNABLE) {
            if ((int32_t)iter->priority > highest_runnable_priority) {
                // Found a new highest priority runnable task
                //printf("New best!\n");
                highest_runnable_priority = iter->priority;
                highest_priority_runnable_task = iter;
            }
        }

        if (iter->next == NULL) {
            break;
        }
        iter = iter->next;
    }
    assert(highest_priority_runnable_task != NULL, "Failed to find a highest priority runnable task");
    return highest_priority_runnable_task;
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

static void _setup_fds(task_small_t* new_task) {
    new_task->fd_table = array_l_create();
    
    // Standard input stream
    new_task->stdin_stream = std_stream_create();
    fd_entry_t* stdin_entry = kmalloc(sizeof(fd_entry_t));
    memset(stdin_entry, 0, sizeof(fd_entry_t));
    stdin_entry->type = STD_TYPE;
    stdin_entry->payload = new_task->stdin_stream;
    array_l_insert(new_task->fd_table, stdin_entry);

    // Standard output stream
    new_task->stdout_stream = std_stream_create();
    fd_entry_t* stdout_entry = kmalloc(sizeof(fd_entry_t));
    memset(stdout_entry, 0, sizeof(fd_entry_t));
    stdout_entry->type = STD_TYPE;
    stdout_entry->payload = new_task->stdout_stream;
    array_l_insert(new_task->fd_table, stdout_entry);

    // Standard error stream
    new_task->stderr_stream = std_stream_create();
    fd_entry_t* stderr_entry = kmalloc(sizeof(fd_entry_t));
    memset(stderr_entry, 0, sizeof(fd_entry_t));
    stderr_entry->type = STD_TYPE;
    stderr_entry->payload = new_task->stderr_stream;
    array_l_insert(new_task->fd_table, stderr_entry);
}

task_small_t* _thread_create(void* entry_point) {
    task_small_t* new_task = kmalloc(sizeof(task_small_t));
    memset(new_task, 0, sizeof(task_small_t));
    new_task->id = next_pid++;
    new_task->blocked_info.status = RUNNABLE;
    _setup_fds(new_task);

    uint32_t stack_size = 0x2000;
    char *stack = kmalloc(stack_size);
    memset(stack, 0, stack_size);

    uint32_t* stack_top = (uint32_t *)(stack + stack_size - 0x4); // point to top of malloc'd stack
    if (entry_point) {
        // TODO(PT): We should be able to pass another argument here to the bootstrap function
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
    new_task->priority = PRIORITY_NONE;
    new_task->priority_lock.name = "[Task priority spinlock]";
    return new_task;
}

task_small_t* thread_spawn(void* entry_point) {
    task_small_t* new_thread = _thread_create(entry_point);
    // Make the thread schedulable now
    _tasking_add_task_to_runlist(new_thread);
    return new_thread;
}

task_small_t* task_spawn(void* entry_point, task_priority_t priority, const char* task_name) {
    // Use the internal thread-state constructor so that this task won't get
    // scheduled until we've had a chance to set all of its state
    task_small_t* new_task = _thread_create(entry_point);
    new_task->is_thread = false;

    // a task is simply a thread with its own virtual address space
    // the new task's address space is a clone of the task that spawned it
    vmm_page_directory_t* new_vmm = vmm_clone_active_pdir();
    new_task->vmm = new_vmm;

    // Assign the provided attributes
    new_task->priority = priority;
    new_task->name = task_name;

    // Task is now ready to run - make it schedulable
    _tasking_add_task_to_runlist(new_task);

    return new_task;
}

/*
 * Immediately preempt the running task and begin running the provided one.
 */
void tasking_goto_task(task_small_t* new_task) {
    //assert(new_task != _current_task_small, "new_task == _current task");
    uint32_t now = time();
    new_task->current_timeslice_start_date = now;
    uint32_t task_quantum = TASK_QUANTUM;
    // If we're scheduling the idle task, give it a smaller slice
    // This gives more opportunity for useful work to appear
    if (!strcmp(new_task->name, "idle")) {
        task_quantum = 5;
    }
    new_task->current_timeslice_end_date = now + task_quantum;

    // Ensure that any shared page tables between the kernel and the preempted VMM have an in-sync allocation state
    // This check should no longer be needed, since allocations within the shared kernel pages are always
    // marked within the shared kernel bitmap. 
    // However, keep the check in to ensure this never regresses.
    // vmm_validate_shared_tables_in_sync(vmm_active_pdir(), boot_info_get()->vmm_kernel);

    if (new_task->vmm != vmm_active_pdir()) {
        vmm_load_pdir(new_task->vmm, false);
    }

    // this method will update _current_task_small
    // this method performs the actual context switch and also updates _current_task_small
    context_switch(new_task);
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
void task_switch() {
    if (_task_schedule_disabled) {
        printf("[Schedule] Skipping task-switch because scheduler is disabled\n");
        return;
    }

    task_small_t* previous_task = _current_task_small;
    task_small_t* next_task = _tasking_find_highest_priority_runnable_task();
    tasking_goto_task(next_task);
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
    return _current_task_small != 0 && pit_callback != 0;
}

static void tasking_timer_tick() {
    kernel_begin_critical();
    if (time() > _current_task_small->current_timeslice_end_date) {
        task_switch();
    }
}

void tasking_unblock_task_with_reason(task_small_t* task, bool run_immediately, task_state reason) {
    // Is this a reason why we're blocked?
    if (!(task->blocked_info.status & reason)) {
        printf("tasking_unblock_task_with_reason(%s, %d) called with reason the task is not blocked for (%d)\n", task->name, reason, task->blocked_info.status);
        assert(0, "invalid call to tasking_unblock_task_with_reason");
        return;
    }
    if (task == _current_task_small) {
        printf("Current task unblocked while running with reason %d\n", reason);
        assert(0, "current task unblocked while running??");
        return;
    }
    // Record why we unblocked
    task->blocked_info.unblock_reason = reason;
    task->blocked_info.status = RUNNABLE;
    if (run_immediately) {
        tasking_goto_task(task);
    }
}

void tasking_block_task(task_small_t* task, task_state blocked_state) {
    // Some states are invalid "blocked" states
    if (blocked_state == RUNNABLE || blocked_state == ZOMBIE) {
        panic("Invalid blocked state");
    }
    task->blocked_info.status = blocked_state;
    // If the current task just became blocked, switch to another
    if (task == _current_task_small) {
        task_switch();
    }
}

void update_blocked_tasks() {
    while (1) {
        // TODO(PT): Lock scheduler list now
        task_small_t* task = _task_list_head;
        while (task) {
            if (task->blocked_info.status == RUNNABLE) {
                task = task->next;
                continue;
            }
            else if (task->blocked_info.status == PIT_WAIT) {
                if (time() > task->blocked_info.wake_timestamp) {
                    tasking_unblock_task_with_reason(task, false, PIT_WAIT);
                }
            }
            else if (task->blocked_info.status == KB_WAIT) {
                if (task->stdin_stream->buf->count > 0) {
                    tasking_unblock_task_with_reason(task, false, KB_WAIT);
                }
            }
            else if (task->blocked_info.status == MOUSE_WAIT) {

            }
            else if (task->blocked_info.status == AMC_AWAIT_MESSAGE) {
                // Will be unblocked by AMC
            }
            else if (task->blocked_info.status == VMM_MODIFY) {
                // Will be unblocked when the client is done modifying the VAS
            }
            else if (task->blocked_info.status == ZOMBIE) {
                // We should start a job to clean up this task
            }
            else {
                printf("PID [%d] is blocked with an unknown reason: %d\n", task->id, task->blocked_info.status);
                panic("unknown block reason");
            }
            task = task->next;
        }
        sys_yield(RUNNABLE);
    }
}

void iosentinel_check_now() {
    tasking_goto_task(_iosentinel_task);
}

void idle_task() {
    while (1) {
        asm("hlt");
        sys_yield(RUNNABLE);
    }
}

void tasking_init() {
    if (tasking_is_active()) {
        panic("called tasking_init() after it was already active");
        return;
    }

    // create first task
    // for the first task, the entry point argument is thrown away. Here is why:
    // on a context_switch, context_switch saves the current runtime state and stores it in the preempted task's context field.
    // when the first context switch happens and the first process is preempted, 
    // the runtime state will be whatever we were doing after tasking_init returns.
    // so, anything we set to be restored in this first task's setup state will be overwritten when it's preempted for the first time.
    // thus, we can pass anything for the entry point of this first task, since it won't be used.
    _current_task_small = thread_spawn(NULL);
    _current_task_small->name = "bootstrap";
    //strncpy(_current_task_small->name, "bootstrap", 10);
    _task_list_head = _current_task_small;
    tasking_goto_task(_current_task_small);

    task_spawn(idle_task, PRIORITY_IDLE, "idle");
    //_iosentinel_task = task_spawn(update_blocked_tasks, PRIORITY_NONE);

    printf_info("Multitasking initialized");
    pit_callback = timer_callback_register((void*)tasking_timer_tick, 10, true, 0);
    asm("sti");
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
	printk("[%d] SBRK 0x%08x\n", getpid(), increment);

	if (increment < 0) {
		ASSERT(0, "sbrk w/ neg increment");
		return NULL;
	}

	task_small_t* current = tasking_get_current_task();
	char* brk = (char*)current->sbrk_current_break;

	if (increment == 0) {
		return brk;
	}

    while (current->sbrk_current_break + increment >= current->sbrk_current_page_head) {
        uint32_t next_page = current->sbrk_current_page_head;
        current->sbrk_current_page_head += PAGE_SIZE;
        if (vmm_address_is_mapped(vmm_active_pdir(), next_page)) {
            // TODO(PT): Is it an error if growing the sbrk region encounters an already-mapped page?
            printk("SBRK grew to cover an already-mapped page 0x%08x\n", next_page);
            continue;
        }
        vmm_alloc_page_address(vmm_active_pdir(), next_page, true);
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
    return _current_first_responder;
}

void become_first_responder_pid(int pid) {
    task_small_t* task = tasking_get_task_with_pid(pid);
    if (!task) {
        printk("become_first_responder_pid(%d) failed\n", pid);
        return;
    }

    _current_first_responder = task;

    /*
    //check if this task already exists in stack of responders
    for (int i = 0; i < responder_stack->size; i++) {
        task_t* tmp = array_m_lookup(responder_stack, i);
        if (tmp == first_responder_task) {
            //remove task so we can add it again
            //this is to ensure responder stack only has unique tasks
            array_m_remove(responder_stack, i);
        }
    }

    //append this task to stack of responders
    array_m_insert(responder_stack, first_responder_task);
    */
}

void become_first_responder() {
    become_first_responder_pid(getpid());
}

void resign_first_responder() {
    Deprecated();
    /*
    if (!first_responder_task) return;
    //if (current_task != first_responder_task) return;

    //remove current first responder from stack of responders
    int last_idx = responder_stack->size - 1;
    task_t* removed = array_m_lookup(responder_stack, last_idx);
    ASSERT(removed == first_responder_task, "top of responder stack wasn't first responder!");

    array_m_remove(responder_stack, last_idx);

    if (responder_stack->size) {
        //set first responder to new head of stack
        first_responder_task = array_m_lookup(responder_stack, responder_stack->size - 1);
    }
    else {
        first_responder_task = NULL;
    }
    */
}

void tasking_print_processes(void) {
    printk("-----------------------proc-----------------------\n");

    if (!_task_list_head) {
        return NULL;
    }
    task_small_t* iter = _task_list_head;
    for (int i = 0; i < MAX_TASKS; i++) {
        printk("[%d] %s ", iter->id, iter->name);
            if (iter == _current_task_small) {
                printk("(active)");
            }

            if (iter == _current_task_small) {
                printk("(active for %d ms more) ", iter->current_timeslice_end_date - time());
            }
            else {
                printk("(inactive since %d ms ago)", time() - iter->current_timeslice_end_date);
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