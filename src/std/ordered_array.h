#ifndef ORDERED_ARRAY_H
#define ORDERED_ARRAY_H

#include "common.h"
#include "mutable_array.h"

//this array is insertion sorted
//it always remains in a sorted state between calls

//predicate should return non-zero if first argument is less than the second
//else return zero
typedef int8_t (*lessthan_predicate_t)(type_t, type_t);
typedef struct {
	lessthan_predicate_t less_than;
	mutable_array_t array;
} ordered_array_t;

//standard less than predicate
int8_t standard_lessthan_predicate(type_t a, type_t b);

//create ordered array
ordered_array_t create_ordered_array(uint32_t max_size, lessthan_predicate_t less_than);
ordered_array_t place_ordered_array(void* addr, uint32_t max_size, lessthan_predicate_t less_than);

//destroy ordered array
void destroy_ordered_array(ordered_array_t* array);

//add item to array
void insert_ordered_array(type_t item, ordered_array_t* array);

//lookup item at index i
type_t lookup_ordered_array(uint32_t i, ordered_array_t* array);

//deletes item at location i from the array
void remove_ordered_array(uint32_t i, ordered_array_t* array);

#endif
