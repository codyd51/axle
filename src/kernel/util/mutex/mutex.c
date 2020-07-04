#include "mutex.h"
#include <std/kheap.h>

//atomically test if *ptr == expected
//if so, set *ptr to new
//else, do nothing
static char cmp_swap(int *ptr, int expected, int new_val) {
	unsigned char ret;

	//note: sete sets a byte, not the word
	asm volatile("		\
		lock;		\
		cmpxchgl %2, %1;\
		sete %0;	\
	" : "=q" (ret), "=m" (*ptr)
	: "r" (new_val), "m" (*ptr), "a" (expected)
	: "memory");
	return ret;
}

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

lock_t* lock_create() {
	lock_t* ret = (lock_t*)kmalloc(sizeof(lock_t));
	ret->flag = 0;
	return ret;
}

void lock(lock_t* lock) {
	if (!lock) return;
	if (!tasking_is_active()) return;
	while (cmp_swap(&lock->flag, 0, 1) == 1) {
		//spin
		;
		asm("pause");
	}
}

void unlock(lock_t* lock) {
	if (!lock) return;
	if (!tasking_is_active()) return;
	barrier();
	lock->flag = 0;
}
