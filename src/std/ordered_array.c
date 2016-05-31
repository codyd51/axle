#include "ordered_array.h"
#include "std.h"

int8_t standard_lessthan_predicate(type_t a, type_t b) {
	return (a < b) ? 1 : 0;
}

ordered_array_t array_o_create(uint32_t max_size, lessthan_predicate_t less_than) {
	ordered_array_t ret;
	ret.array = array_m_create(max_size);
	ret.size = ret.array.size;
	ret.less_than = less_than;
	return ret;
}

ordered_array_t array_o_place(void* addr, uint32_t max_size, lessthan_predicate_t less_than) {
	ordered_array_t ret;
	ret.array = array_m_place(addr, max_size);
	ret.size = ret.array.size;
	ret.less_than = less_than;
	return ret;
}

void array_o_destroy(ordered_array_t* array) {
	array_m_destroy(&(array->array));
}

void array_o_insert(type_t item, ordered_array_t* array) {
	array_m_insert(item, &(array->array));
	array->size = array->array.size;
}

type_t array_o_lookup(uint32_t i, ordered_array_t* array) {
	return array_m_lookup(i, &(array->array));
}

uint16_t array_o_index(type_t item, ordered_array_t* array) {
	return array_m_index(item, &(array->array));
}

void array_o_remove(uint32_t i, ordered_array_t* array) {
	array_m_remove(i, &(array->array));
	array->size = array->array.size;
}

