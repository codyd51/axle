#include "mutex.h"
#include <std/kheap.h>
#include <kernel/multitasking/tasks/task_small.h>

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
		printf("Proc %d found contended lock 0x%08x %d at %d\n", getpid(), lock, lock->flag, contention_start);
		if (getpid() == 0 && (uint32_t)lock > 0xff000000) {
			panic("root proc contended");
		}
	}
    while (!atomic_compare_exchange(&lock->flag, 0, 1)) {
		asm("pause");
    }
	if (contention_start) {
		printf("Prod %d received contended lock 0x%08x after %d ticks\n", getpid(), lock, time() - contention_start);
	}
}

void unlock(lock_t* lock) {
	if (!lock) return;
	if (!tasking_is_active()) return;
    atomic_store(&lock->flag, 0);
}
