#include <kernel/assert.h>
#include <kernel/multitasking/tasks/task_small.h>

#include "spinlock.h"

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

static inline uint8_t atomic_bit_test_and_set(uint32_t* val) {
    uint8_t ret;
    asm volatile (
            "LOCK bts $0, %0;"
            "     setc    %1;"
            : "+m" (*val), "=qm" (ret)
            :
            : "cc", "memory");
    return ret;
}

static inline void local_irq_disable(void) {
    asm volatile (
        "cli"
        ::
        :"cc", "memory"
    );
}

static inline void local_irq_enable(void) {
    asm volatile (
        "sti"
        ::
        :"cc", "memory"
    );
}

static inline union x86_rflags get_rflags(void) {
    union x86_rflags flags;
    asm volatile(
        "pushfq;"
        "popq %0;"
        :"=rm"(flags.raw)
        :
    );
    return flags;
}

static inline void set_rflags(union x86_rflags flags) {
    asm volatile(
        "pushq %0;"
        "popfq;"
        :
        :"g"(flags.raw)
        :"cc", "memory"
    );
}

/*
 * Disable interrupts, but restore the original %rflags
 * interrupt enable flag (IF) state afterwards.
 */

static inline union x86_rflags local_irq_disable_save(void) {
    union x86_rflags flags = get_rflags();
    if (flags.__packed.irqs_enabled) {
        local_irq_disable();
    }
    return flags;
}

static inline void local_irq_restore(union x86_rflags flags) {
    if (flags.__packed.irqs_enabled) {
        //set_rflags(flags);
        asm("sti");
    }
}

void spinlock_acquire(spinlock_t* lock) {
    if (!lock) return;
    assert(lock->name, "Spinlock was used without assigning a name");

    /*
    // Keep track of whether we had to wait to acquire the lock
	uint32_t contention_start = 0;

	if (lock->flag != 0) {
        // Check for data corruption
        if (lock->flag != 1) {
            assert(lock->flag == 1, "Bad lock flag");
        }

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
        *

        // If we're already holding the lock, increment the 'nested acquire' count
		contention_start = time();
        if (lock->owner_pid == getpid()) {
            //printf("Spinlock: [%d] did nested acquire of %s at %d\n", getpid(), lock->name, contention_start);
            lock->nest_count += 1;
            return;
        }

		//printf("Spinlock: [%d] found contended spinlock with flag %d at %d: %s owned by %d\n", getpid(), lock->flag, contention_start, lock->name, lock->owner_pid);
        //assert(false, "contended spinlock");
	}

    // Spin until the lock is released
    while (!atomic_compare_exchange(&lock->flag, 0, 1)) {
        //task_switch();
    }
     */

    union x86_rflags rflags = local_irq_disable_save();
    while (atomic_bit_test_and_set(&lock->flag) == 1) {
        //local_irq_restore(rflags);
        // Wait until the lock looks unlocked before retrying
        while (lock->flag == 1) {
	        //asm volatile ("pause":::"memory");
        }
        //local_irq_disable();
    }
    lock->rflags = rflags;
    lock->owner_pid = getpid();

    /*
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
		//printf("Spinlock: *** Proc %d received contended lock 0x%08x %s after %d ticks\n", getpid(), lock, lock->name, time() - contention_start);
	}
    else {
        //printf("Spinlock: Proc %d received uncontended lock 0x%08x %s\n", getpid(), lock, lock->name);
    }
    */
    //spinlock_acquire_spin(lock);
}

void spinlock_release(spinlock_t* lock) {
    if (!lock) return;
    /*
    if (lock->nest_count) {
        lock->nest_count -= 1;
        //printf("Spinlock: [%d] decrement nested acquire of %s to %d\n", getpid(), lock->name, lock->nest_count);
        return;
    }
    atomic_store(&lock->flag, 0);
    // Allow other contexts on this processor to interact with the spinlock
    if (lock->interrupts_enabled_before_acquire) {
        asm("sti");
    }
    //printf("Spinlock: Proc %d freed lock 0x%08x %s\n", getpid(), lock, lock->name);
    */

    union x86_rflags rflags = lock->rflags;
    asm volatile ("":::"memory");
    lock->flag = 0;
    local_irq_restore(rflags);
    //spinlock_release_spin(lock);
}
