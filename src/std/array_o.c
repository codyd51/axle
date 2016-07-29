#include "array_o.h"
#include "std.h"

int8_t standard_lessthan_predicate(type_t a, type_t b) {
	return (a < b) ? 1 : 0;
}

array_o* array_o_create(uint32_t max_size, lessthan_predicate_t less_than) {
	array_o* ret = (array_o*)kmalloc(sizeof(array_o));
	ret->array = array_m_create(max_size);
	ret->size = ret->array->size;
	ret->less_than = less_than;
	return ret;
}

array_o* array_o_place(void* addr, uint32_t max_size, lessthan_predicate_t less_than) {
	array_o* ret = (array_o*)kmalloc(sizeof(array_o));
	ret->array = array_m_place(addr, max_size);
	ret->size = ret->array->size;
	ret->less_than = less_than;
	return ret;
}

void array_o_destroy(array_o* array) {
	array_m_destroy(array->array);
}

void array_o_insert(array_o* array, type_t item) {
	ASSERT(array->less_than, "ordered array didn't have a less-than predicate!");
	uint32_t iterator = 0;
	while (iterator < array->size && array->less_than(array_m_lookup(array->array, iterator), item)) {
		iterator++;
	}
	//just add item ot end of array
	if (iterator == array->size) {
		array_m_insert(array->array, item);
		array->size = array->array->size;
	}
	else {
		//TODO implement mutable_array function to add item at index
		//shifting all larger indexed elements by one, as below
		type_t tmp = array->array->array[iterator];
		array->array->array[iterator] = item;
		while (iterator < array->size) {
			iterator++;
			type_t tmp2 = array->array->array[iterator];
			array->array->array[iterator] = tmp;
			tmp = tmp2;
		}
		array->array->size++;
		array->size++;
	}
}

type_t array_o_lookup(array_o* array, uint32_t i) {
	return array_m_lookup(array->array, i);
}

uint16_t array_o_index(array_o* array, type_t item) {
	return array_m_index(array->array, item);
}

void array_o_remove(array_o* array, uint32_t i) {
	array_m_remove(array->array, i);
	array->size = array->array->size;
}

