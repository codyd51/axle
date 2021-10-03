#ifndef ARRAY_H
#define ARRAY_H

#include <stdint.h>

#define ARR_NOT_FOUND -1
typedef void* type_t;

typedef struct array {
	type_t* array;
	int32_t size;
	int32_t max_size;
} array_t;

array_t* array_create(int32_t max_size);
void array_destroy(array_t* array);

void array_insert(array_t* array, type_t item);
void array_set(array_t* array, int32_t i, type_t item);
void array_remove(array_t* array, int32_t i);

type_t array_lookup(array_t* array, int32_t i);
int32_t array_index(array_t* array, type_t item);

#endif