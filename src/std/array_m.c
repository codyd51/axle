#include "array_m.h"
#include "std.h"
#include <kernel/util/spinlock/spinlock.h>

array_m* array_m_create(int32_t max_size) {
	array_m* ret = (array_m*)kmalloc(sizeof(array_m));
	memset(ret, 0, sizeof(array_m));
	ret->size = 0;
	ret->max_size = max_size;
    ret->array = (type_t*)calloc(max_size, sizeof(type_t));
	ret->lock.name = "array_m_lock";
	assert(ret->lock.flag == 0, "Lock flag was not zero on alloc");
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
	if (array->size + 1 >= array->max_size) {
		printf("Array overflow: %s %d\n", array->lock.name, array->max_size);
	}
	ASSERT(array->size + 1 <= array->max_size, "array would exceed max_size (%d)", array->max_size);

	// Add item to array
	array->array[array->size++] = item;
}

static type_t _array_m_lookup_unlocked(array_m* array, int32_t i) {
	ASSERT(i < array->size && i >= 0, "index (%d) was out of bounds (%d)", i, array->size - 1);

	return array->array[i];
}

static int32_t _array_m_index_unlocked(array_m* array, type_t item) {
	for (int32_t i = 0; i < array->size; i++) {
		if (_array_m_lookup_unlocked(array, i) == item) return i;
	}
	return -1;
}

static void _array_m_remove_unlocked(array_m* array, int32_t i) {
	if (i >= array->size) {
		printf("Removed index is out-of-bounds: %d larger than size %d\n", i, array->size);
		panic("Array index is out-of-bounds");
	}
	if (i < 0) panic("Negative array index");

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
	spinlock_acquire(&array->lock);
	_array_m_insert_unlocked(array, item);
	spinlock_release(&array->lock);
}

int32_t array_m_index(array_m* array, type_t item) {
	spinlock_acquire(&array->lock);
	int32_t ret = _array_m_index_unlocked(array, item);
	spinlock_release(&array->lock);
	return ret;
}

type_t array_m_lookup(array_m* array, int32_t i) {
	spinlock_acquire(&array->lock);
	type_t ret = _array_m_lookup_unlocked(array, i);
	spinlock_release(&array->lock);
	return ret;
}

void array_m_remove(array_m* array, int32_t i) {
	spinlock_acquire(&array->lock);
	_array_m_remove_unlocked(array, i);
	spinlock_release(&array->lock);
}