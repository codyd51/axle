#ifndef STD_MUTABLE_ARRAY_H
#define STD_MUTABLE_ARRAY_H

#include "std_base.h"
#include <stdint.h>

__BEGIN_DECLS

typedef void* type_t;

typedef struct {
	type_t* array;
	uint32_t size;
	uint32_t max_size;
} mutable_array_t;

//create mutable array
STDAPI mutable_array_t array_m_create(uint32_t max_size);
STDAPI mutable_array_t array_m_place(void* addr, uint32_t max_size);

//destroy mutable array
STDAPI void array_m_destroy(mutable_array_t* array);

//add item to array
STDAPI void array_m_insert(type_t item, mutable_array_t* array);

//lookup item at index i
STDAPI type_t array_m_lookup(uint32_t i, mutable_array_t* array);

//find index of item
STDAPI uint32_t array_m_index(type_t item, mutable_array_t* array);

//deletes item at location i from the array
STDAPI void array_m_remove(uint32_t i, mutable_array_t* array);

__END_DECLS

#endif
