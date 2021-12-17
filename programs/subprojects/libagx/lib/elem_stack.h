#ifndef ELEM_STACK_H
#define ELEM_STACK_H

#include <stdint.h>

typedef struct stack_elem {
	void* payload;
	struct stack_elem* prev;
	struct stack_elem* next;
} stack_elem_t;

typedef struct elem_stack {
	stack_elem_t* head;
	stack_elem_t* tail;
	uint32_t count;
} elem_stack_t;

elem_stack_t* stack_create(void);
void stack_destroy(elem_stack_t* s);

void stack_push(elem_stack_t* stack, void* item);
void* stack_peek(elem_stack_t* stack);
void* stack_pop(elem_stack_t* stack);

void* stack_get(elem_stack_t* stack, uint32_t idx);

#endif