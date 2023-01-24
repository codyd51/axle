#include "mlfq.h"
#include <std/array_l.h>
#include <std/array_m.h>
#include <std/math.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/drivers/pit/pit.h>

#define MLFQ_QUEUE_COUNT 4
#define MLFQ_BOOST_INTERVAL 600

const int _mlfq_quantums[MLFQ_QUEUE_COUNT] = {10, 20, 30, 40};

typedef struct mlfq_ent {
    task_small_t* task;
    uint32_t last_schedule_start;
    uint32_t ttl_remaining;
} mlfq_ent_t;

typedef struct mlfq_queue {
    uint32_t quantum;
    array_l* round_robin_tasks;
    spinlock_t spinlock;
} mlfq_queue_t;

static array_m* _queues = 0;

void mlfq_init(void) {
    _queues = array_m_create(MLFQ_QUEUE_COUNT);
    for (uint32_t i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        mlfq_queue_t* q = kcalloc(1, sizeof(mlfq_queue_t));
        q->round_robin_tasks = array_l_create();
        q->quantum = _mlfq_quantums[i];
        q->spinlock.name = "MLFQ queue spinlock";
        printf("MLFQ queue %d quantum = %dms\n", i, q->quantum);
        array_m_insert(_queues, q);
    }
}

void mlfq_add_task_to_queue(task_small_t* task, uint32_t queue_idx) {
    assert(queue_idx < MLFQ_QUEUE_COUNT, "Invalid queue provided");
    // Tasks are always enqueued to the highest priority queue
    //printf("adding to queue %d (size: %d)\n", queue_idx, _queues->size);
    mlfq_queue_t* queue = array_m_lookup(_queues, queue_idx);
    mlfq_ent_t* ent = kcalloc(1, sizeof(mlfq_ent_t));
    ent->task = task;
    ent->ttl_remaining = queue->quantum;
    array_l_insert(queue->round_robin_tasks, ent);
    //printf("MLFQ added task [%d %s] to q %d idx %d\n", task->id, task->name, queue_idx, array_l_index(queue->round_robin_tasks, ent));
}

bool mlfq_choose_task(task_small_t** out_task, uint32_t* out_quantum) {
    uintptr_t current_cpu_id = cpu_id();
    // Start at the high-priority queues and make our way down
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        mlfq_queue_t* q = array_m_lookup(_queues, i);
        for (int j = 0; j < q->round_robin_tasks->size; j++) {
            mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, j);
            if (ent->task->blocked_info.status == RUNNABLE && !ent->task->is_currently_executing /*&& ent->task->cpu_id == cpu_id()*/) {
                *out_task = ent->task;
                *out_quantum = ent->ttl_remaining;
                ent->last_schedule_start = ms_since_boot();
                //printf("MLFQ %d: [%d %s] Schedule, ttl = %d @ %dms\n", ms_since_boot(), ent->task->id, ent->task->name, ent->ttl_remaining, ent->last_schedule_start);
                return true;
            }
        }
    }
    // Didn't find any runnable task
    return false;
}

static bool _find_task(task_small_t* task, uint32_t* out_queue_idx, uint32_t* out_ent_idx) {
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        mlfq_queue_t* q = array_m_lookup(_queues, i);
        for (int j = 0; j < q->round_robin_tasks->size; j++) {
            mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, j);
            if (ent->task == task) {
                *out_queue_idx = i;
                *out_ent_idx = j;
                return true;
            }
        }
    }
    return false;
}

bool mlfq_next_quantum_for_task(task_small_t* task, uint32_t* out_quantum) {
    // Start at the high-priority queues and make our way down
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        mlfq_queue_t* q = array_m_lookup(_queues, i);
        for (int j = 0; j < q->round_robin_tasks->size; j++) {
            mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, j);
            if (ent->task == task) {
                *out_quantum = ent->ttl_remaining;
                ent->last_schedule_start = ms_since_boot();
                return true;
            }
        }
    }
    // Didn't find any runnable task
    return false;
}

void mlfq_delete_task(task_small_t* task) {
    uint32_t queue_idx = 0;
    uint32_t entry_idx = 0;
    if (!_find_task(task, &queue_idx, &entry_idx)) {
        printf("mlfq_delete_task failed: didn't find provided task in any queue\n");
        return;
    }
    mlfq_queue_t* q = array_m_lookup(_queues, queue_idx);
    spinlock_acquire(&q->spinlock);

    printf("Removing task [%d %s] from MLFQ scheduler pool. Found in Q%d idx %d\n", task->id, task->name, queue_idx, entry_idx);
    mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, entry_idx);
    array_l_remove(q->round_robin_tasks, entry_idx);
    kfree(ent);

    spinlock_release(&q->spinlock);
}

bool mlfq_priority_boost_if_necessary(void) {
    if (ms_since_boot() % 1000 == 0) {
        mlfq_queue_t* high_prio = array_m_lookup(_queues, 0);
        spinlock_acquire(&high_prio->spinlock);

        int runnable_count = 0;
        int orig_high_prio_size = high_prio->round_robin_tasks->size;
        
        for (int i = 1; i < MLFQ_QUEUE_COUNT; i++) {
            mlfq_queue_t* q = array_m_lookup(_queues, i);
            spinlock_acquire(&q->spinlock);

            while (q->round_robin_tasks->size > 0) {
                //printf("remove from %d (size %d)\n", i, q->round_robin_tasks->size);
                mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, 0);
                //printf("\tMLFQ Q%d boost [%d %s]\n", i, ent->task->id, ent->task->name);
                array_l_remove(q->round_robin_tasks, 0);
                ent->ttl_remaining = high_prio->quantum;
                if (ent->task->blocked_info.status == RUNNABLE) runnable_count++;
                array_l_insert(high_prio->round_robin_tasks, ent);
            }

            spinlock_release(&q->spinlock);
        }

        //printf("MLFQ %d: Did priority-boost (high prio %d -> %d, runnable count: %d)\n", ms_since_boot(), orig_high_prio_size, high_prio->round_robin_tasks->size, runnable_count);
        if (ms_since_boot() % 10000 == 0) {
            mlfq_print();
        }
        spinlock_release(&high_prio->spinlock);
        return true;
    }
    return false;
}

bool mlfq_prepare_for_switch_from_task(task_small_t* task) {
    // Find the task within the queues
    uint32_t queue_idx = 0;
    uint32_t ent_idx = 0;
    if (!_find_task(task, &queue_idx, &ent_idx)) {
        return false;
    }

    // Should the task remain in its current queue?
    // Move the task to the back of its queue
    // TODO(PT): Drop to a lower queue if we've exceeded our life
    mlfq_queue_t* q = array_m_lookup(_queues, queue_idx);
    spinlock_acquire(&q->spinlock);

    mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, ent_idx);

    uint32_t runtime = ms_since_boot() - ent->last_schedule_start;
    int32_t ttl_remaining = (int32_t)ent->ttl_remaining - runtime;
    //printf("MLFQ %d (int %d): [%d %s] prepare_for_switch_from (last start %d, ttl %d, queue %d, runtime %d)\n", ms_since_boot(), interrupts_enabled(), ent->task->id, ent->task->name, ent->last_schedule_start, ent->ttl_remaining, queue_idx, runtime);
    if (ttl_remaining <= 0) {
        array_l_remove(q->round_robin_tasks, ent_idx);
        // If we're already on the lowest queue, replenish TTL and do nothing
        if (queue_idx == MLFQ_QUEUE_COUNT - 1) {
            //printf("MLFQ: [%d %s] Already on lowest queue\n", ent->task->id, ent->task->name);
            ent->ttl_remaining = q->quantum;
            array_l_insert(q->round_robin_tasks, ent);
        }
        else {
            // Lifetime has expired - demote to lower queue
            //printf("MLFQ: [%d %s] Demoting to lower queue %d, TTL expired %d last_starat %d now %d\n", ent->task->id, ent->task->name, queue_idx + 1, ttl_remaining, ent->last_schedule_start, ms_since_boot());
            mlfq_queue_t* new_queue = array_m_lookup(_queues, queue_idx + 1);
            ent->ttl_remaining = new_queue->quantum;
            array_l_insert(new_queue->round_robin_tasks, ent);
        }
    }
    else {
        // Keep on the same queue and decrement TTL
        ent->ttl_remaining = ttl_remaining;
        //printf("MLFQ: [%d %s] Decrementing TTL to %d\n", ent->task->id, ent->task->name, ttl_remaining);
    }

    spinlock_release(&q->spinlock);

    return true;
}

void mlfq_print(void) {
    printf("MLFQ %d\n", ms_since_boot());
    for (int i = 0; i < MLFQ_QUEUE_COUNT; i++) {
        mlfq_queue_t* q = array_m_lookup(_queues, i);
        if (!q->round_robin_tasks->size) continue;
        printf("\tQ%d: ", i);
        for (int j = 0; j < q->round_robin_tasks->size; j++) {
            mlfq_ent_t* ent = array_l_lookup(q->round_robin_tasks, j);
            const char* blocked_reason = "unknown";
            switch (ent->task->blocked_info.status) {
                case RUNNABLE:
                    blocked_reason = "run";
                    break;
                case IRQ_WAIT:
                    blocked_reason = "irq";
                    break;
                case AMC_AWAIT_MESSAGE:
                    blocked_reason = "amc";
                    break;
                case (IRQ_WAIT | AMC_AWAIT_MESSAGE):
                    blocked_reason = "adi";
                    break;
                default:
                    blocked_reason = "unknown";
                    break;
            }
            printf("[[Cpu%d,Pid%d] %s %s] ", ent->task->cpu_id, ent->task->id, ent->task->name, blocked_reason);
        }
        printf("\n");
    }
}
