#include "callback.h"
#include <libutils/array.h>
#include <stdlib.h>

#define CALLBACK_LIST_MAX_CALLBACKS 64

typedef struct callback {
    void* ctx;
    bool (*is_callback_satisfied)(void*);
    void (*complete_callback)(void*);
} callback_t;

void* callback_list_init(void) {
    array_t* callbacks = array_create(CALLBACK_LIST_MAX_CALLBACKS);
    return callbacks;
}

void* callback_list_destroy(void* callback_list) {
    array_destroy(callback_list);
}

void callback_list_add_callback(
    void* callback_list_opaque,
    void* callback_info,
    cb_is_satisfied_func is_satisfied,
    cb_complete_func complete
) {
    array_t* callback_list = (array_t*)callback_list_opaque;
    callback_t* cb = calloc(1, sizeof(callback_t));
    cb->ctx = callback_info;
    cb->is_callback_satisfied = is_satisfied;
    cb->complete_callback = complete;
    array_insert(callback_list, cb);
}

void callback_list_invoke_ready_callbacks(void* callback_list_opaque) {
    array_t* callback_list = (array_t*)callback_list_opaque;

    int callback_indexes_to_complete[callback_list->size];
    int completed_callback_count = 0;

    for (int i = 0; i < callback_list->size; i++) {
        callback_t* cb = array_lookup(callback_list, i);
        if (cb->is_callback_satisfied(cb->ctx)) {
            callback_indexes_to_complete[completed_callback_count++] = i;
        }
    }

    // Go from high indexes to low indexes to keep the indexes the same as we remove elements
    for (int i = completed_callback_count-1; i >= 0; i--) {
        int idx = callback_indexes_to_complete[i];
        callback_t* cb = array_lookup(callback_list, idx);
        array_remove(callback_list, idx);

        // Dispatch the callback
        cb->complete_callback(cb->ctx);
        free(cb);
    }
}

bool callback_list_invoke_callback_if_ready(void* callback_list_opaque, void* ctx) {
    array_t* callback_list = (array_t*)callback_list_opaque;
    // First, find the mentioned callback
    for (int i = 0; i < callback_list->size; i++) {
        callback_t* cb = array_lookup(callback_list, i);
        if (cb->ctx == ctx) {
            if (cb->is_callback_satisfied(cb->ctx)) {
                array_remove(callback_list, i);
                // Dispatch the callback
                cb->complete_callback(cb->ctx);
                free(cb);
                return true;
            }
            return false;
        }
    }
    // We didn't find the provided callback at all
    return false;
}
