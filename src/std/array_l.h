#ifndef STD_ARRAY_L_H
#define STD_ARRAY_L_H

#include "std_base.h"
#include <kernel/assert.h>
#include <kernel/util/mutex/mutex.h>
#include <stdint.h>
#include "std.h"

__BEGIN_DECLS

typedef void* type_t;

typedef struct array_l_item {
	type_t item;
	struct array_l_item* next;
} array_l_item;

typedef struct {
	array_l_item* head;
	int32_t size;
	lock_t lock;
} array_l;

//create array list
STDAPI array_l* array_l_create();

//destroy array list
STDAPI void array_l_destroy(array_l* array);

//add item to array list
STDAPI void array_l_insert(array_l* array, type_t item);

//lookup item at index idx
STDAPI type_t array_l_lookup(array_l* array, int32_t idx);

//find index of item
STDAPI int32_t array_l_index(array_l* array, type_t item);

//deletes item at location i from the array
STDAPI void array_l_remove(array_l* array, int32_t i);

__END_DECLS

#endif
