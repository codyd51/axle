#include <memory.h>
#include <stdlib.h>

#include "array.h"

array_t* array_create(int32_t max_size) {
	array_t* ret = (array_t*)calloc(1, sizeof(array_t));
	ret->size = 0;
	ret->max_size = max_size;
    ret->array = (type_t*)calloc(max_size, sizeof(type_t));
	return ret;
}

void array_destroy(array_t* array) {
	free(array->array);
	free(array);
}

void array_insert(array_t* array, type_t item) {
	// Make sure we can't go over the allocated size
	if (array->size + 1 >= array->max_size) printf("*** array would exceed max size! %d\n", array->max_size);
	if (array->size + 1 >= array->max_size) {
		// Force a page fault so we get a stack trace
		char* v = 0x0;
		*v = 0xdeadbeef;
	}
	assert(array->size + 1 <= array->max_size, "array would exceed max_size");
	// Add item to array
	array->array[array->size++] = item;
}

void array_remove(array_t* array, int32_t i) {
	if (i >= array->size) {
		printf("Removed index is out-of-bounds: %d larger than size %d\n", i, array->size);
		assert(0, "Array index is out-of-bounds");
	}
	if (i < 0) assert(0, "Negative array index");

	//shift back all elements
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;
}

type_t array_lookup(array_t* array, int32_t i) {
	if (i >= array->size || i < 0) printf("Bad index: %d\n", i);
	assert(i < array->size && i >= 0, "index was out of bounds");
	return array->array[i];
}

int32_t array_index(array_t* array, type_t item) {
	for (int32_t i = 0; i < array->size; i++) {
		if (array_lookup(array, i) == item) return i;
	}
	return -1;
}
