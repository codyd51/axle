#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct spinlock_t {
	int flag;
	const char* name;
	bool interrupts_enabled_before_acquire;
	int owner_pid;
	int nest_count;
} spinlock_t;

void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#endif
