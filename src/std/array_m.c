#include "array_m.h"
#include "std.h"
#include <kernel/util/mutex/mutex.h>

static lock_t* mutex;

array_m* array_m_create(int32_t max_size) {
	mutex = lock_create();

	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	ret->size = 0;
	ret->max_size = max_size;
    ret->array = (type_t*)calloc(max_size, sizeof(type_t));
	return ret;
}

array_m* array_m_place(void* addr, int32_t max_size) {
	mutex = lock_create();

	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	ret->size = 0;
	ret->max_size = max_size;
	ret->array = (type_t)addr;
	memset(ret->array, 0, max_size * sizeof(type_t));
	return ret;
}

void array_m_destroy(array_m* array) {
	kfree(array);
}

void array_m_insert(array_m* array, type_t item) {
	lock(mutex);

	// Make sure we can't go over the allocated size
	ASSERT(array->size + 1 <= array->max_size, "array would exceed max_size (%d)", array->max_size);

	// Add item to array
	array->array[array->size++] = item;

	unlock(mutex);
}

int32_t array_m_index(array_m* array, type_t item) {
	//TODO optimize this
	for (int32_t i = 0; i < array->size; i++) {
		if (array_m_lookup(array, i) == item) return i;
	}
	return -1;
}

void array_m_remove(array_m* array, int32_t i) {
	lock(mutex);

	ASSERT(i < array->size && i >= 0, "can't remove object at index (%d) in array with (%d) elements", i, array->size);

	//shift back all elements
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;

	unlock(mutex);
}
