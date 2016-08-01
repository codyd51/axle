#include "array_m.h"
#include "std.h"
#include <kernel/util/mutex/mutex.h>

static lock_t* mutex;

array_m* array_m_create(uint32_t max_size) {
	mutex = lock_create();

	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	ret->array = (void*)kmalloc(max_size * sizeof(type_t));
	memset(ret->array, 0, max_size * sizeof(type_t));
	ret->size = 0;
	ret->max_size = max_size;
	return ret;
}

array_m* array_m_place(void* addr, uint32_t max_size) {
	mutex = lock_create();

	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	ret->array = (type_t*)addr;
	memset(ret->array, 0, max_size * sizeof(type_t));
	ret->size = 0;
	ret->max_size = max_size;
	return ret;
}

void array_m_destroy(array_m* array) {
	kfree(array);
}

void array_m_insert(array_m* array, type_t item) {
	lock(mutex);

	// Make sure we can't go over the allocated size
	ASSERT(array->size < array->max_size, "array would exceed max_size");

	// Add item to array
	array->array[array->size++] = item;

	unlock(mutex);
}

type_t array_m_lookup(array_m* array, uint32_t i) {
	ASSERT(i < array->size, "index was out of bounds");

	return array->array[i];
}

uint32_t array_m_index(array_m* array, type_t item) {
	//TODO optimize this
	for (int i = 0; i < array->size; i++) {
		if (array_m_lookup(array, i) == item) return i;
	}
	return -1;
}

void array_m_remove(array_m* array, uint32_t i) {
	lock(mutex);

	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;

	unlock(mutex);
}
