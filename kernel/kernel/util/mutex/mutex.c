#include <std/kheap.h>
#include <std/printf.h>

#include <kernel/assert.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/drivers/rtc/clock.h>

#include "mutex.h"

lock_t* lock_create() {
	Deprecated();
	return NULL;
}

static inline bool atomic_compare_exchange(int* ptr, int compare, int exchange) {
    return __atomic_compare_exchange_n(ptr, &compare, exchange,
            0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

static inline void atomic_store(int* ptr, int value) {
    __atomic_store_n(ptr, 0, __ATOMIC_SEQ_CST);
}

static inline int atomic_add_fetch(int* ptr, int d) {
    return __atomic_add_fetch(ptr, d, __ATOMIC_SEQ_CST);
}

void lock(lock_t* lock) {
	if (!lock) return;
	if (!tasking_is_active()) return;
	uint32_t contention_start = 0;
	if (lock->flag != 0) {
		contention_start = time();
		//printf("Mutex: [%d] found contended %s %d at %d\n", getpid(), lock->name ?: "lock", lock->flag, contention_start);
		assert(lock->flag == 1, "Bad lock flag");
		printf("Mutex: [%d] found contended lock %d at %d\n", getpid(), lock->flag, contention_start);
		printf("Contended lock 0x%08x %s (int enabled? %d)\n", lock->name, lock->name, interrupts_enabled());
		assert(0, "Contended lock");
	}
    while (!atomic_compare_exchange(&lock->flag, 0, 1)) {
		asm("pause");
    }
	if (contention_start) {
		printf("Prod %d received contended lock 0x%08x after %d ticks\n", getpid(), lock, time() - contention_start);
	}
	//printf("PID [%d] took lock %d\n", getpid(), lock->flag);
}

void unlock(lock_t* lock) {
	if (!lock) return;
	if (!tasking_is_active()) return;
    atomic_store(&lock->flag, 0);
}
