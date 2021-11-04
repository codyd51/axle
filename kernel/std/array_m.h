#ifndef STD_ARRAY_M_H
#define STD_ARRAY_M_H

#include "std_base.h"
#include <kernel/assert.h>
#include <kernel/util/spinlock/spinlock.h>
#include <stdint.h>

__BEGIN_DECLS

#define ARR_NOT_FOUND -1

typedef void* type_t;

typedef struct {
	type_t* array;
	int32_t size;
	int32_t max_size;
	spinlock_t lock;
} array_m;

//create mutable array
STDAPI array_m* array_m_create(int32_t max_size);

//destroy mutable array
STDAPI void array_m_destroy(array_m* array);

//add item to array
STDAPI void array_m_insert(array_m* array, type_t item);

//lookup item at index i
STDAPI type_t array_m_lookup(array_m* array, int32_t i);

//find index of item
STDAPI int32_t array_m_index(array_m* array, type_t item);

//deletes item at location i from the array
STDAPI void array_m_remove(array_m* array, int32_t i);

__END_DECLS

#endif
