#ifndef MUTABLE_ARRAY_H
#define MUTABLE_ARRAY_H

#include "common.h"

typedef void* type_t;

typedef struct {
	type_t* array;
	uint32_t size;
	uint32_t max_size;
} mutable_array_t;

//create mutable array
mutable_array_t create_mutable_array(uint32_t max_size);
mutable_array_t place_mutable_array(void* addr, uint32_t max_size);

//destroy mutable array
void destroy_mutable_array(mutable_array_t* array);

//add item to array
void insert_mutable_array(type_t item, mutable_array_t* array);

//lookup item at index i
type_t lookup_mutable_array(uint32_t i, mutable_array_t* array);

//deletes item at location i from the array
void remove_mutable_array(uint32_t i, mutable_array_t* array);

#endif
