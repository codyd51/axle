#ifndef MUTEX_H
#define MUTEX_H

typedef struct lock_t {
	int flag;
} lock_t;

lock_t* lock_create();
void lock(lock_t* lock);
void unlock(lock_t* lock);

#endif
