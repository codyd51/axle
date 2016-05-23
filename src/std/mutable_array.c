#include "mutable_array.h"
#include "std.h"

mutable_array_t create_mutable_array(uint32_t max_size) {
	mutable_array_t ret;
	ret.array = (void*)kmalloc(max_size * sizeof(type_t));
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	return ret;
}

mutable_array_t place_mutable_array(void* addr, uint32_t max_size) {
	mutable_array_t ret;
	ret.array = (type_t*)addr;
	memset(ret.array, 0, max_size * sizeof(type_t));
	ret.size = 0;
	ret.max_size = max_size;
	return ret;
}

void destroy_mutable_array(mutable_array_t* array) {

}

void insert_mutable_array(type_t item, mutable_array_t* array) {
	uint32_t iterator = 0;
	while (iterator < array->size) {
		iterator++;
	}
	if (iterator == array->size) {
		//just add item to end of array
		array->array[array->size++] = item;
	}
	else {
		//shift all other items in array
		type_t tmp = array->array[iterator];
		array->array[iterator] = item;
		while (iterator < array->size) {
			iterator++;
			type_t tmp2 = array->array[iterator];
			array->array[iterator] = tmp;
			tmp = tmp2;
		}
		array->size++;
	}
}

type_t lookup_mutable_array(uint32_t i, mutable_array_t* array) {
	ASSERT(i < array->size);

	return array->array[i];
}

void remove_mutable_array(uint32_t i, mutable_array_t* array) {
	while (i < array->size) {
		array->array[i] = array->array[i + 1];
		i++;
	}
	array->size--;
}
