#include "mutable_array.h"
#include "std.h"

mutable_array_t array_m_create(uint32_t max_size) {
	mutable_array_t ret;
	ret.array = (void*)kmalloc(max_size * sizeof(type_t));
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	return ret;
}

mutable_array_t array_m_place(void* addr, uint32_t max_size) {
	mutable_array_t ret;
	ret.array = (type_t*)addr;
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	return ret;
}

void array_m_destroy(mutable_array_t* array) {

}

void array_m_insert(type_t item, mutable_array_t* array) {
	// Make sure we can't go over the allocated size
	ASSERT(array->size < array->max_size, "array would exceed max_size");

	// Add item to array
	array->array[array->size++] = item;
}

type_t array_m_lookup(uint32_t i, mutable_array_t* array) {
	ASSERT(i < array->size, "index was out of bounds");

	return array->array[i];
}

uint32_t array_m_index(type_t item, mutable_array_t* array) {
	//TODO optimize this
	for (int i = 0; i < array->size; i++) {
		if (array_m_lookup(i, array) == item) return i;
	}
	return -1;
}

void array_m_remove(uint32_t i, mutable_array_t* array) {
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;
}
