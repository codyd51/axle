#include "gfx.h"
#include <std/std.h>

// dynamic_array
// Thanks to http://stackoverflow.com/questions/3536153/c-dynamically-growing-array

dynamic_array* dynamic_array_create(uint32_t initialSize) {
	dynamic_array *ret;
	ret->array = (void*)kmalloc(initialSize * sizeof(type_t));
	ret->used = 0;
	ret->size = initialSize;
	return ret;
}

void dynamic_array_insert(dynamic_array *array, type_t element) {
	if (array->used == array->size) {
		array->size *= 2;
		array->array = (void*)realloc(array->array, array->size * sizeof(type_t));
	}
	array->array[array->used++] = element;
}

void dynamic_array_free(dynamic_array *array) {
	kfree(array->array);
	array->array = NULL;
	array->used = array->size = 0;
}