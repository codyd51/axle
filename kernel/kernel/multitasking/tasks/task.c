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
}

task_t* task_list() {
    Deprecated();
    return NULL;
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
}

void list_task(task_small_t* task) {
    Deprecated();
}

void block_task_context(task_t* task, task_state_t reason, void* context) {
    Deprecated();
}

void block_task(task_t* task, task_state_t reason) {
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
    return NULL;
}

task_t* task_with_pid(int pid) {
    Deprecated();
    return NULL;
}

task_t* task_current() {
    Deprecated();
    return NULL;
}

static void _tasking_register_process(task_small_t* task) {
    Deprecated();
}

void idle() {
    Deprecated();
}

void destroy_task(task_t* task) {
    Deprecated();
}

void reap_task(task_t* tmp) {
    Deprecated();
}

void iosent() {
    Deprecated();
}

void enqueue_task(task_small_t* task, int queue) {
    Deprecated();
}

void dequeue_task(task_small_t* task) {
    Deprecated();
}

void switch_queue(task_small_t* task, int new) {
    Deprecated();
}

void demote_task(task_small_t* task) {
    Deprecated();
}

void promote_task(task_small_t* task) {
    Deprecated();
}

bool tasking_is_active_old() {
    Deprecated();
    return false;
}

void booster() {
    Deprecated();
}

void tasking_install() {
    Deprecated();
}

void tasking_installed() {
    Deprecated();
}

static void _create_task_queues(mlfq_option options) {
    Deprecated();
}

void tasking_init_old(mlfq_option options) {
    Deprecated();
}

void update_blocked_tasks_old() {
    Deprecated();
}

void int_wait(int irq) {
    Deprecated();
}

int fork_old(char* name) {
    Deprecated();
    return 0;
}

task_small_t* first_queue_runnable(array_m* queue, int offset) {
    Deprecated();
    return NULL;
}

array_m* first_queue_containing_runnable(void) {
    Deprecated();
    return NULL;
}

task_small_t* mlfq_schedule() {
    Deprecated();
    return NULL;
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
    return NULL;
}

void force_enumerate_blocked() {
    Deprecated();
}

void jump_user_mode() {
    Deprecated();
}

int waitpid(int pid, int* status, int options) {
    Deprecated();
    return -1;
}

int wait(int* status) {
    Deprecated();
    return -1;
}

Window* task_register_window(Rect frame) {
    Deprecated();
    return NULL;
}
