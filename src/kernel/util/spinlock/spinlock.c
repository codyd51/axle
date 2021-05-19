#include "spinlock.h"
#include <kernel/multitasking/tasks/task_small.h>

static inline bool atomic_compare_exchange(int* ptr, int compare, int exchange) {
    return __atomic_compare_exchange_n(ptr, &compare, exchange,
            false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline void atomic_store(int* ptr, int value) {
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}

static inline int atomic_add_fetch(int* ptr, int d) {
    return __atomic_add_fetch(ptr, d, __ATOMIC_SEQ_CST);
}

void spinlock_acquire(spinlock_t* lock) {
    if (!lock) return;
    assert(lock->name, "Spinlock was used without assigning a name");

    // Keep track of whether we had to wait to acquire the lock
	uint32_t contention_start = 0;

	if (lock->flag != 0) {
        // Check for data corruption
		assert(lock->flag == 1, "Bad lock flag");

        // We should never be waiting on a spinlock while interrupts are disabled
        // (This means another task has acquired the spinlock, 
        // and since interrupts are disabled we will never context switch back to the 
        // other task)
        /*
        if (!interrupts_enabled()) { 
            if (lock->owner_pid != getpid()) {
                printf("Spinlock %s held by another consumer while interrupts are disabled. Owner: [%d]\n", lock->name, lock->owner_pid); 
                task_small_t* owner = tasking_get_task_with_pid(lock->owner_pid);
                printf("Owner: %d %s\n", lock->owner_pid, owner->name);
                assert(0, "Spinlock held by another process while interrupts are disabled\n");
            }
        }
        */

        // If we're already holding the lock, increment the 'nested acquire' count
		contention_start = time();
        if (lock->owner_pid == getpid()) {
            printf("Spinlock: [%d] did nested acquire of %s at %d\n", getpid(), lock->name, contention_start);
            lock->nest_count += 1;
            return;
        }

		printf("Spinlock: [%d] found contended spinlock with flag %d at %d: %s owned by %d\n", getpid(), lock->flag, contention_start, lock->name, lock->owner_pid);
	}

    // Spin until the lock is released
    while (!atomic_compare_exchange(&lock->flag, 0, 1)) {
		asm("pause");
    }

    // Prevent another context on this processor from acquiring the spinlock
    // atomic_store(&lock->interrupts_enabled_before_acquire, interrupts_enabled());
    lock->interrupts_enabled_before_acquire = interrupts_enabled();
    if (lock->interrupts_enabled_before_acquire) {
        asm("cli");
    }

    // Ensure it's really ours
    assert(lock->flag == 1, "Lock was not properly acquired");
    lock->owner_pid = getpid();

	if (contention_start) {
		printf("Spinlock: *** Proc %d received contended lock 0x%08x %s after %d ticks\n", getpid(), lock, lock->name, time() - contention_start);
	}
    else {
        //printf("Spinlock: Proc %d received uncontended lock 0x%08x %s\n", getpid(), lock, lock->name);
    }
}

void spinlock_release(spinlock_t* lock) {
    if (!lock) return;
    if (lock->nest_count) {
        lock->nest_count -= 1;
        printf("Spinlock: [%d] decrement nested acquire of %s to %d\n", getpid(), lock->name, lock->nest_count);
        return;
    }
    lock->owner_pid = -1;
    atomic_store(&lock->flag, 0);
    // Allow other contexts on this processor to interact with the spinlock
    if (lock->interrupts_enabled_before_acquire) {
        asm("sti");
    }
    //printf("Spinlock: Proc %d freed lock 0x%08x %s\n", getpid(), lock, lock->name);
}
