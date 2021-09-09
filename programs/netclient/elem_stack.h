#ifndef ELEM_STACK_H
#define ELEM_STACK_H

typedef struct elem_stack elem_stack_t;

elem_stack_t* stack_create(void);
void stack_destroy(elem_stack_t* s);

void stack_push(elem_stack_t* stack, void* item);
void* stack_peek(elem_stack_t* stack);
void* stack_pop(elem_stack_t* stack);

#endif