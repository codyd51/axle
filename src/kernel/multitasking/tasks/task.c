#include "task.h"
#include <std/std.h>
#include <std/math.h>
#include <std/memory.h>
#include <kernel/drivers/kb/kb.h>
#include <kernel/interrupts/interrupts.h>
#include <kernel/vmm/vmm.h>
#include <kernel/multitasking//util.h>
#include <kernel/syscall//sysfuncs.h>
#include <kernel/drivers/rtc/clock.h>
#include <std/klog.h>
#include <kernel/util/mutex/mutex.h>
#include "record.h"
#include <gfx/lib/gfx.h>
#include <user/xserv/xserv.h>
#include <kernel/multitasking//pipe.h>
#include <kernel/multitasking//std_stream.h>
#include <kernel/multitasking//fd.h>
#include <kernel/util/shmem/shmem.h>
#include <kernel/boot_info.h>
#include <kernel/segmentation/gdt_structures.h>
#include "task_small.h"

//function defined in asm which returns the current instruction pointer
uint32_t read_eip();
//defined in asm
//performs actual task switch
void task_switch_real(uint32_t eip, uint32_t paging_dir, uint32_t ebp, uint32_t esp);

//magic value placed in eax at end of task switch
//we read eax when trying to catch current eip
//if this value is in eax, we know we already caught eip and that the task switch is over, so it should return immediately
#define STACK_MAGIC 0xDEADBEEF

#define MAX_TASKS 128

#define MLFQ_DEFAULT_QUEUE_COUNT 16
#define MLFQ_MAX_QUEUE_LENGTH 16

#define HIGH_PRIO_QUANTUM 5
#define BOOSTER_PERIOD 1000

#define MAX_RESPONDERS 32

static int next_pid = 1;
task_t* current_task = 0;
static array_m* queues = 0;
static array_m* queue_lifetimes = 0;
static task_t* active_list = 0;

task_t* first_responder_task = 0;
static array_m* responder_stack = 0;

static lock_t* mutex = 0;

void enqueue_task(task_small_t* task, int queue);
void dequeue_task(task_small_t* task);

void stdin_read(char* buf, uint32_t count);
void stdout_read(char* buffer, uint32_t count);
void stderr_read(char* buffer, uint32_t count);
static void setup_fds(task_t* task) {
    Deprecated();
    memset(&task->fd_table, 0, sizeof(fd_entry_t) * FD_MAX);

    //initialize backing std stream
    task->std_stream = std_stream_create();

    //set up stdin/out/err to point to task's std stream
    //this stream backs all 3 descriptors
    /*
    fd_entry std;
    std.type = STD_TYPE;
    std.payload = task->std_stream;

    task->fd_table[0] = std;
    task->fd_table[1] = std;
    task->fd_table[2] = std;
    */
}

task_t* task_list() {
    return active_list;
}

static bool is_dead_task_crit(task_t* task) {
    Deprecated();
}

static void tasking_critical_fail() {
    Deprecated();
}

void kill_task(task_t* task) {
    Deprecated();
}

void _kill() {
    Deprecated();
}

void goto_pid(int id, bool update_current_task_state);
void unlist_task(task_small_t* task) {
    Deprecated();

    //if task to unlist is head, move head
    if (task == active_list) {
        active_list = task->next;
    }
    else {
        //walk linked list
        task_small_t* prev = active_list;
        task_small_t* current = prev;
        while (current && current->next != NULL) {
            if (current == task) {
                break;
            }
            prev = current;
            current = current->next;
        }
        //did we find it?
        if (task != current) {
            printk("unlist_task() couldn't unlist %s\n", task->name);
            return;
        }

        //remove from list
        prev->next = current->next;
    }
}

void list_task(task_small_t* task) {
    Deprecated();

    //walk linked list
    task_small_t* current = active_list;
    while (current->next != NULL) {
        if (task == current) {
            return;
        }
        current = current->next;
    }

    //extend list
    current->next = task;
}

void block_task_context(task_t* task, task_state reason, void* context) {
    Deprecated();
}

void block_task(task_t* task, task_state reason) {
    Deprecated();
}

void unblock_task(task_t* task) {
    Deprecated();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
task_t* create_process(char* name, uint32_t eip, bool wants_stack) {
    Deprecated();
}
#pragma GCC diagnostic pop

task_t* task_with_pid_auth(int pid) {
    Deprecated();
    //first, ensure this task is allowed to do this!
    //permission to use task_with_pid is controlled by the PROC_MASTER_PERMISSION flag
    //only check if this is a non-kernel task
    //check for .bss segment as heuristic for whether this is an external program
    if (current_task->prog_break) {
        if (!(current_task->permissions & PROC_MASTER_PERMISSION)) {
            printf_err("%s[%d] is not authorized to use task_with_pid!", current_task->name, getpid());
            return NULL;
        }
    }
    //operation permitted
    return task_with_pid(pid);
}

task_t* task_with_pid(int pid) {
    Deprecated();
}

task_t* task_current() {
    return current_task;
}

static void _tasking_register_process(task_small_t* task) {
    Deprecated();

    if (!tasking_is_active()) return;

    list_task(task);

    //all new tasks are placed on highest priority queue
    // XXX(PT): disabling queues for lower-level tasking work. Scheduling is round-robing for now
    // enqueue_task(task, 0);
}

void idle() {
    Deprecated();

    while (1) {
        //nothing to do!
        //put the CPU to sleep until the next interrupt
        asm volatile("hlt");
        //once we return from above, go to next task
        sys_yield(RUNNABLE);
    }
}

void destroy_task(task_t* task) {
    Deprecated();
}

void reap_task(task_t* tmp) {
    Deprecated();
}

void iosent() {
    Deprecated();
    while (1) {
        update_blocked_tasks();
        //yield cpu to next task
        sys_yield(RUNNABLE);
    }
}

void enqueue_task(task_small_t* task, int queue) {
    Deprecated();

    lock(mutex);
    if (queue < 0 || queue >= queues->size) {
        ASSERT(0, "Tried to insert %s into invalid queue %d", task->name, queue);
    }

    array_m* raw = array_m_lookup(queues, queue);

    //ensure task does not already exist in this queue
    if (array_m_index(raw, task) == ARR_NOT_FOUND) {
        lock(mutex);
        array_m_insert(raw, task);
        unlock(mutex);

        task->queue = queue;
        //new queue, reset lifespan
        task->lifespan = 0;
    }
    else {
        printf_err("Tried to enqueue %s onto queue where it already existed (%d)", task->name, queue);
    }
    unlock(mutex);
}

void dequeue_task(task_small_t* task) {
    Deprecated();

    lock(mutex);
    if (task->queue < 0 || task->queue >= queues->size) {
        ASSERT(0, "Tried to remove %s from invalid queue %d", task->name, task->queue);
    }
    array_m* raw = array_m_lookup(queues, task->queue);

    int idx = array_m_index(raw, task);
    if (idx < 0) {
        printf_err("Tried to dequeue %s from queue %d it didn't belong to!", task->name, task->queue);
        //fall back on searching all queues for this task
        for (int i = 0; i < queues->size; i++) {
            array_m* queue = array_m_lookup(queues, i);
            for (int j = 0; j < queue->size; j++) {
                task_t* tmp = array_m_lookup(queue, j);
                if (task == tmp) {
                    //found task we were looking for
                    printf_info("Task was actually in queue %d", i);
                    array_m_remove(queue, j);
                    unlock(mutex);

                    return;
                }
            }
        }
        //never found the task!
        printf_err("Task %s did not exist in any queues!", task->name);
        return;
    }

    array_m_remove(raw, idx);
    unlock(mutex);

    //if for some reason this task is still in the queue (if it was added to queue twice),
    //dequeue it again
    if (array_m_index(raw, task) != ARR_NOT_FOUND) {
        dequeue_task(task);
    }
}

void switch_queue(task_small_t* task, int new) {
    Deprecated();

    dequeue_task(task);
    enqueue_task(task, new);
}

void demote_task(task_small_t* task) {
    Deprecated();

    //if we're already at the bottom task, don't attempt to demote further
    if (task->queue >= queues->size - 1) {
        return;
    }
    switch_queue(task, task->queue + 1);
}

void promote_task(task_small_t* task) {
    Deprecated();

    switch_queue(task, task->queue - 1);
}

bool tasking_is_active_old() {
    Deprecated();

    //return (queues && queues->size >= 1 && current_task);
    return current_task != 0;
}

void booster() {
    Deprecated();

    task_t* tmp = active_list;
    while (tmp) {
        switch_queue(tmp, 0);
        tmp = tmp->next;
    }
}

void tasking_install() {
    Deprecated();
}

void tasking_installed() {
    Deprecated();
}

static void _create_task_queues(mlfq_option options) {
    Deprecated();

    int queue_count = 0;
    switch (options) {
        case LOW_LATENCY:
            queue_count = 1;
            break;
        case PRIORITIZE_INTERACTIVE:
        default:
            queue_count = MLFQ_DEFAULT_QUEUE_COUNT;
            break;
    }

    queues = array_m_create(queue_count + 1);
    for (int i = 0; i < queue_count; i++) {
        array_m* queue = array_m_create(MLFQ_MAX_QUEUE_LENGTH);
        array_m_insert(queues, queue);
    }

    queue_lifetimes = array_m_create(queue_count + 1);
    for (int i = 0; i < queue_count; i++) {
        array_m_insert(queue_lifetimes, (type_t)(HIGH_PRIO_QUANTUM * (i + 1)));
    }
}

void tasking_init_old(mlfq_option options) {
    Deprecated();
}

void update_blocked_tasks_old() {
    Deprecated();
    if (!tasking_is_active()) return;

    //if there is a pending key, wake first responder
    /*
    if (haskey() && first_responder_task->state == KB_WAIT) {
        unblock_task(first_responder_task);
        goto_pid(first_responder_task->id);
    }
    */

    //wake blocked tasks if the event they were blocked for has occurred
    //TODO is this optimizable?
    //don't look through every queue, use linked list of tasks
    task_t *task = active_list;
    while (task)
    {
        if (task->std_stream->buf->count && task->state == KB_WAIT)
        {
            unblock_task(task);
            goto_pid(task->id, true);
        }
        else if (task->state == PIT_WAIT)
        {
            if (time() >= task->wake_timestamp)
            {
                unblock_task(task);
            }
        }
        //TODO figure out when exactly tasks with MOUSE_WAIT should be unblocked
        else if (task->state == MOUSE_WAIT)
        {
            unblock_task(task);
            goto_pid(task->id, true);
        }
        else if (task->state == CHILD_WAIT)
        {
            //search if any of this task's children are zombies
            for (int i = 0; i < task->child_tasks->size; i++)
            {
                task_t *child = array_m_lookup(task->child_tasks, i);
                if (child->state == ZOMBIE)
                {
                    //found a zombie!
                    //wake parent
                    unblock_task(task);
                    break;
                }
            }
        }
        else if (task->state == PIPE_FULL)
        {
            pipe_block_info *info = (pipe_block_info *)task->block_context;
            pipe_t *waiting = info->pipe;
            int free_bytes = waiting->cb->capacity - waiting->cb->count;
            if (free_bytes >= info->free_bytes_needed)
            {
                //space has freed up in the pipe
                //we can now unblock
                unblock_task(task);
            }
        }
        else if (task->state == PIPE_EMPTY)
        {
            pipe_t *waiting = task->block_context;
            if (waiting->cb->count > 0)
            {
                //pipe now has data we can read
                //we can now unblock
                unblock_task(task);
            }
        }
        else if (task->state == ZOMBIE)
        {
            if (task->parent)
            {
                if (task->parent->state != CHILD_WAIT)
                {
                    //printk("parent %d isn't waiting for dangling child %d\n", task->parent->id, task->id);
                }
            }
        }
        else if (task->state == IRQ_WAIT)
        {
            if (task->irq_satisfied)
            {
                task->irq_satisfied = false;
                unblock_task(task);
            }
        }

        task = task->next;
    }
}

void int_wait(int irq) {
    Deprecated();
}

int fork_old(char* name) {
    Deprecated();
    if (!tasking_is_active()) {
        panic("called fork() before tasking was active");
        return 0;
    }

    kernel_begin_critical();

    //keep reference to parent for later
    task_t* parent = current_task;

    task_t* child = create_process(name, 0, false);

    //copy all file descriptors from parent to child
    /*
    for (int i = 0; i < FD_MAX; i++) {
        fd_entry entry = parent->fd_table[i];
        if (fd_empty(entry)) continue;

        fd_add_index(child, entry, i);
        if (entry.type == PIPE_TYPE) {
            pipe_t* pipe = (pipe_t*)entry.payload;
            //and add this new child to the pipe's reference list
            array_m_insert(pipe->pids, (type_t)child->id);
        }
    }
    */

    _tasking_register_process(child);

    //set parent process of newly created process to currently running task
    child->parent = parent;
    //insert the newly created child task into the parent's array of children
    if (!parent->child_tasks) {
        ASSERT(0, "%s[%d] had no child_task array!\n", parent->name, parent->id);
    }
    if (parent->child_tasks->size < parent->child_tasks->max_size) {
        array_m_insert(parent->child_tasks, child);
    }
    else {
        ASSERT(0, "fork() child_tasks was full!\n");
    }

    //THIS LINE will be the entry point for child process
    //(as read_eip will give us the address of this line)
    uint32_t eip = read_eip();

    //eip check above is the entry point when the child starts executing
    //therefore, we could either be the parent or child
    //check!
    if (current_task == parent) {
        //still parent task
        //set up esp/ebp/eip for child
        uint32_t esp, ebp;
        asm volatile("mov %%esp, %0" : "=r"(esp));
        asm volatile("mov %%ebp, %0" : "=r"(ebp));
        child->esp = esp;
        child->ebp = ebp;
        child->eip = eip;

        kernel_end_critical();

        //return child PID by convention
        return child->id;
    }
    else {
        kernel_end_critical();
        //now executing child process
        //return 0 by convention
        return 0;
    }
}

task_small_t* first_queue_runnable(array_m* queue, int offset) {
    Deprecated();

    for (int i = offset; i < queue->size; i++) {
        task_small_t* tmp = array_m_lookup(queue, i);
        if (tmp->blocked_info.status == RUNNABLE) {
            return tmp;
        }
    }
    //no runnable tasks within this queue!
    return NULL;
}

array_m* first_queue_containing_runnable(void) {
    Deprecated();

    //we could look at every queue individually, but that would be slow
    //let's take advantage of our linked list of tasks and search that
    task_small_t* curr = active_list;
    task_small_t* highest_prio_runnable = NULL;

    //TODO figure out why this block doesn't work
    while (curr) {
        if (curr->blocked_info.status == RUNNABLE) {
            //if this task has a higher priority (lower queue #), or this is the first runnable task we've found,
            //mark it as best
            if (!highest_prio_runnable || curr->queue < highest_prio_runnable->queue) {
                highest_prio_runnable = curr;
            }
        }
        curr = curr->next;
    }

    array_m* queue = array_m_lookup(queues, highest_prio_runnable->queue);
    if (!highest_prio_runnable || highest_prio_runnable->blocked_info.status != RUNNABLE || !queue->size) {
        //if (1) {
        //printf_err("Couldn't find runnable task in linked list of tasks!");
        for (int i = 0; i < queues->size; i++) {
            array_m* tmp = array_m_lookup(queues, i);
            if (first_queue_runnable(tmp, 0) != NULL) {
                return tmp;
            }
        }
    }
    //no queues contained any runnable tasks!
    if (!highest_prio_runnable) {
        proc();
        ASSERT(highest_prio_runnable, "No queues contained any runnable tasks!");
    }

    return queue;
}

task_small_t* mlfq_schedule() {
    Deprecated();

    if (!tasking_is_active()) {
        panic("called mlfq_schedule() before tasking was active");
        return NULL;
    }

    //find current index in queue
    array_m* current_queue = array_m_lookup(queues, current_task->queue);
    int current_task_idx = array_m_index(current_queue, current_task);
    if (current_task_idx < 0) {
        ASSERT(0, "Couldn't find current task in queue %d", current_task->queue);
    }

    //increment lifespan by how long this task ran
    if (current_task->relinquish_date && current_task->begin_date) {
        uint32_t current_runtime = (current_task->relinquish_date - current_task->begin_date);
        current_task->lifespan += current_runtime;
        sched_record_usage(current_task, current_runtime);
    }

    if (current_task->lifespan >= (uint32_t)array_m_lookup(queue_lifetimes, current_task->queue)) {
        demote_task(current_task);
    }

    //if we're running in low-latency mode, save time by just using round-robin
    if (queues->size == 1) {
        //attempt to save time by first looking at the next task in linked list
        task_small_t* next = current_task->next;
        if (!next) next = active_list;
        while (next->blocked_info.status != RUNNABLE) {
            next = next->next;
            if (!next) {
                next = active_list;
            }
        }
        ASSERT(next, "Couldn't find valid runnable task!");
        return next;
    }

    //find first non-empty queue
    array_m* new_queue = first_queue_containing_runnable();
    if (!new_queue->size) {
        proc();
    }
    ASSERT(new_queue->size, "Couldn't find any queues with tasks to run in queue %d!", array_m_index(queues, new_queue));

    if (new_queue->size >= 1) {
        //round-robin through this queue

        //if this is the same queue as the previous task, start at that index
        if (current_queue == new_queue) {
            //if this is the last index, loop around to the start of the array
            if (current_task_idx + 1 >= new_queue->size) {
                task_small_t* valid = first_queue_runnable(new_queue, 0);
                if (valid != NULL) {
                    return valid;
                }
            }
            //return task at the next index
            task_small_t* valid = first_queue_runnable(new_queue, current_task_idx + 1);
            if (valid != NULL) {
                return valid;
            }
        }

        //we're on a new queue
        //start from the first task in it
        task_small_t* valid = first_queue_runnable(new_queue, 0);
        if (valid != NULL) {
            return valid;
        }
    }
    ASSERT(0, "Couldn't find task to switch to!");
}

void goto_pid(int id, bool update_current_task_state) {
    Deprecated();
}

uint32_t task_switch_old(bool update_current_task_state) {
    Deprecated();
    return 0;
}

void handle_pit_tick() {
    Deprecated();
}

void proc() {
    Deprecated();

    printk("-----------------------proc-----------------------\n");

    for (int i = 0; i < queues->size; i++) {
        array_m* queue = array_m_lookup(queues, i);
        for (int j = 0; j < queue->size; j++) {
            task_small_t* task = array_m_lookup(queue, j);
            uint32_t runtime = (uint32_t)array_m_lookup(queue_lifetimes, task->queue);
            printk("[%d Q %d] %s %s", task->id, task->queue, task->name, (task == get_first_responder()) ? "(FR)" : "");
            if (task == current_task) {
                printk("(active)");
            }
            else {
                printk("used");
            }
            printk(" %d/%d ms ", task->lifespan, runtime);

            switch (task->blocked_info.status) {
                case RUNNABLE:
                printk("(runnable)");
                break;
                case KB_WAIT:
                printk("(blocked by keyboard)");
                break;
                case PIT_WAIT:
                printk("(blocked by timer, wakes %d)", task->blocked_info.wake_timestamp);
                break;
                case MOUSE_WAIT:
                printk("(blocked by mouse)");
                break;
                case ZOMBIE:
                printk("(zombie)");
                break;
                case CHILD_WAIT:
                printk("(blocked by child)");
                break;
                case PIPE_EMPTY:
                case PIPE_FULL:
                printk("(blocked by pipe)");
                break;
                case IRQ_WAIT:
                printk("(blocked by IRQ)");
                break;
                default:
                break;
            }
            printk("\n");
        }
    }
    printk("---------------------------------------------------\n");
}

void force_enumerate_blocked() {
    Deprecated();

    if (!tasking_is_active()) return;
    update_blocked_tasks();
}

void jump_user_mode() {
    Deprecated();

    // Set up a stack structure for switching to user mode.
    // the pop eax, or, and re-push take eflags which was pushed onto the stack,
    // and turns on the interrupt enabled flag
    // this ensures interrupts will be turned back on upon iret, as we do a cli at the
    // beginning of this routine, and can't do an sti once we're done since we're in user mode
    //set_kernel_stack(current_task->kernel_stack + KERNEL_STACK_SIZE);
    asm volatile("  \
    cli; \
    mov $0x23, %ax; \
    mov %ax, %ds; \
    mov %ax, %es; \
    mov %ax, %fs; \
    mov %ax, %gs; \
    \
    mov %esp, %eax; \
    pushl $0x23; \
    pushl %eax; \
    pushf; \
    pop %eax; \
    or %eax, 0x200; \
    push %eax; \
    pushl $0x1B; \
    push $1f; \
    iret; \
    1: \
    ");
}

int waitpid(int pid, int* status, int options) {
    Deprecated();

    task_t* parent = current_task;
    block_task(parent, CHILD_WAIT);

    //wait finished!
    //find child which terminated
    for (int i = 0; i < parent->child_tasks->size; i++) {
        task_t* child = array_m_lookup(parent->child_tasks, i);
        //check if this pid is suitable to wake parent
        //if requested pid is -1, any child is acceptable
        //otherwise, we need exact match
        bool valid_pid = false;
        if (pid == -1 || pid == child->id) {
            valid_pid = true;
        }


        if (child->state == ZOMBIE && valid_pid) {
            int ret = child->exit_code;
            int child_pid = child->id;
            array_m_remove(parent->child_tasks, i);
            reap_task(child);

            if (status) {
                *status = ret;
            }

            //if pid is -1, then we are waiting for all child tasks to complete
            //so, if pid is -1 and there is another child process running,
            //keep waiting
            if (pid == -1 && parent->child_tasks->size) {
                return waitpid(pid, status, options);
            }

            return child_pid;
        }
    }
    ASSERT(0, "parent unblocked but no child terminated!\n");
    return -1;
}

int wait(int* status) {
    Deprecated();
    return waitpid(-1, status, 0);
}

Window* task_register_window(Rect frame) {
    Deprecated();

    //if we're creating a window for a task through xserv_win_create
    //then we're in a syscall handler and getpid() will return the pid of the
    //proc that ran the syscall
    //this is how we know when a user proc is connected to a window
    task_t* current = task_with_pid(getpid());
    Window* win = create_window(frame);
    array_m_insert(current->windows, win);

    return win;
}
