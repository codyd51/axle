#ifndef STD_ORDERED_ARRAY_H
#define STD_ORDERED_ARRAY_H

#include "std_base.h"
#include "mutable_array.h"

__BEGIN_DECLS

//this array is insertion sorted
//it always remains in a sorted state between calls

//predicate should return non-zero if first argument is less than the second
//else return zero
typedef int8_t (*lessthan_predicate_t)(type_t, type_t);
typedef struct {
	lessthan_predicate_t less_than;
	uint16_t size;
	mutable_array_t array;
} ordered_array_t;

//standard less than predicate
STDAPI int8_t standard_lessthan_predicate(type_t a, type_t b);

//create ordered array
STDAPI ordered_array_t array_o_create(uint32_t max_size, lessthan_predicate_t less_than);
STDAPI ordered_array_t array_o_place(void* addr, uint32_t max_size, lessthan_predicate_t less_than);

//destroy ordered array
STDAPI void array_o_destroy(ordered_array_t* array);

//add item to array
STDAPI void array_o_insert(type_t item, ordered_array_t* array);

//lookup item at index i
STDAPI type_t array_o_lookup(uint32_t i, ordered_array_t* array);

//return index of item
STDAPI uint16_t array_o_index(type_t item, ordered_array_t* array);

//deletes item at location i from the array
STDAPI void array_o_remove(uint32_t i, ordered_array_t* array);

__END_DECLS

#endif // STD_ORDERED_ARRAY_H
