#ifndef CALLBACK_H
#define CALLBACK_H

#include <stdbool.h>
#include <stdint.h>

void* callback_list_init(void);
void* callback_list_destroy(void* callback_list);

typedef bool(*cb_is_satisfied_func)(void*);
typedef void(*cb_complete_func)(void*);

// Set up a callback with a satisfaction and completion function
void callback_list_add_callback(
    void* callback_list,
    void* callback_info,
    cb_is_satisfied_func is_satisfied,
    cb_complete_func complete
);

// Invokes the satisfaction function of each callback and completes them if ready
void callback_list_invoke_ready_callbacks(void* callback_list);

bool callback_list_invoke_callback_if_ready(void* callback_list, void* ctx);

#endif