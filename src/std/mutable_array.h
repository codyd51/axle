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
mutable_array_t array_m_create(uint32_t max_size);
mutable_array_t array_m_place(void* addr, uint32_t max_size);

//destroy mutable array
void array_m_destroy(mutable_array_t* array);

//add item to array
void array_m_insert(type_t item, mutable_array_t* array);

//lookup item at index i
type_t array_m_lookup(uint32_t i, mutable_array_t* array);

//find index of item
uint32_t array_m_index(type_t item, mutable_array_t* array);

//deletes item at location i from the array
void array_m_remove(uint32_t i, mutable_array_t* array);

#endif
