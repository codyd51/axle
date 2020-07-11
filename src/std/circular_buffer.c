#include <std/std.h>
#include <std/memory.h>
#include "circular_buffer.h"

void cb_init(circular_buffer *cb, size_t capacity, size_t sz) {
	memset(cb, 0, sizeof(cb));
    cb->buffer = kmalloc(capacity * sz);
    if(cb->buffer == NULL) {
		return;
	}

    cb->buffer_end = (char *)cb->buffer + capacity * sz;
    cb->capacity = capacity;
    cb->count = 0;
    cb->sz = sz;
    cb->head = cb->buffer;
    cb->tail = cb->buffer;
}

void cb_free(circular_buffer *cb) {
    kfree(cb->buffer);
	memset(cb, 0, sizeof(cb));
}

void cb_push_back(circular_buffer *cb, const char *item) {
    if(cb->count == cb->capacity){
		panic("circular buffer at capacity!");
		return;
    }

    memcpy(cb->head, item, cb->sz);
    cb->head = (char*)cb->head + cb->sz;
    if(cb->head == cb->buffer_end) {
        cb->head = cb->buffer;
	}
    cb->count++;
}

void cb_pop_front(circular_buffer *cb, char *item) {
    if(cb->count == 0){
		panic("popping from empty circular_buffer!");
        return;
    }

    memcpy(item, cb->tail, cb->sz);
    cb->tail = (char*)cb->tail + cb->sz;
    if(cb->tail == cb->buffer_end) {
        cb->tail = cb->buffer;
	}
    cb->count--;
}

void cb_peek(circular_buffer *cb, char *item) {
    if(cb->count == 0){
		panic("peeking from empty circular_buffer!");
		return;
    }

    memcpy(item, cb->tail, cb->sz);
}

