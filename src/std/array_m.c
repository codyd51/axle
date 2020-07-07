#include "array_m.h"
#include "std.h"
#include <kernel/util/mutex/mutex.h>

array_m* array_m_create(int32_t max_size) {
	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	memset(ret, 0, sizeof(ret));
	ret->size = 0;
	ret->max_size = max_size;
    ret->array = (type_t*)calloc(max_size, sizeof(type_t));
	return ret;
}

array_m* array_m_place(void* addr, int32_t max_size) {
	Deprecated();
	return NULL;
}

void array_m_destroy(array_m* array) {
	kfree(array);
}

static void _array_m_insert_unlocked(array_m* array, type_t item) {
	// Make sure we can't go over the allocated size
	ASSERT(array->size + 1 <= array->max_size, "array would exceed max_size (%d)", array->max_size);

	// Add item to array
	array->array[array->size++] = item;
}

static int32_t _array_m_index_unlocked(array_m* array, type_t item) {
	for (int32_t i = 0; i < array->size; i++) {
		if (array_m_lookup(array, i) == item) return i;
	}
	return -1;
}

static void _array_m_remove_unlocked(array_m* array, int32_t i) {
	ASSERT(i < array->size && i >= 0, "can't remove object at index (%d) in array with (%d) elements", i, array->size);

	//shift back all elements
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;
}

/*
 * Public API wrappers
 * Enforces mutex on array reads and writes
 */

void array_m_insert(array_m* array, type_t item) {
	lock(&array->lock);
	_array_m_insert_unlocked(array, item);
	unlock(&array->lock);
}

int32_t array_m_index(array_m* array, type_t item) {
	lock(&array->lock);
	_array_m_index_unlocked(array, item);
	unlock(&array->lock);
}

void array_m_remove(array_m* array, int32_t i) {
	lock(&array->lock);
	_array_m_remove_unlocked(array, i);
	unlock(&array->lock);
}