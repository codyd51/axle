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

lock_t* lock_create() {
	/*
	lock_t* ret = (lock_t*)kmalloc(sizeof(lock_t));
	ret->flag = 0;
	return ret;
	*/
	return 0;
}

void lock(lock_t* lock) {
	if (!lock) return;
	while (cmp_swap(&lock->flag, 0, 1) == 1) {
		//spin
		;
	}
}

void unlock(lock_t* lock) {
	if (!lock) return;
	lock->flag = 0;
}
