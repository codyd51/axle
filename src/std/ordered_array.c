#include "ordered_array.h"
#include "std.h"

int8_t standard_lessthan_predicate(type_t a, type_t b) {
	return (a < b) ? 1 : 0;
}

ordered_array_t create_ordered_array(uint32_t max_size, lessthan_predicate_t less_than) {
	ordered_array_t ret;
	ret.array = create_mutable_array(max_size);
	ret.size = ret.array.size;
	ret.less_than = less_than;
	return ret;
}

ordered_array_t place_ordered_array(void* addr, uint32_t max_size, lessthan_predicate_t less_than) {
	ordered_array_t ret;
	ret.array = place_mutable_array(addr, max_size);
	ret.size = ret.array.size;
	ret.less_than = less_than;
	return ret;
}

void destroy_ordered_array(ordered_array_t* array) {
	destroy_mutable_array(&(array->array));
}

void insert_ordered_array(type_t item, ordered_array_t* array) {
	insert_mutable_array(item, &(array->array));
	array->size = array->array.size;
}

type_t lookup_ordered_array(uint32_t i, ordered_array_t* array) {
	return lookup_mutable_array(i, &(array->array));
}

void remove_ordered_array(uint32_t i, ordered_array_t* array) {
	remove_mutable_array(i, &(array->array));
	array->size = array->array.size;
}

