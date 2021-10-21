#ifndef CIRCULAR_BUF_H
#define CIRCULAR_BUF_H

#include <stdint.h>
#include <stddef.h>

typedef struct circular_buffer {
    char *buffer;     // data buffer
    char *buffer_end; // end of data buffer
    size_t capacity;  // maximum number of items in the buffer
    size_t count;     // number of items in the buffer
    size_t sz;        // size of each item in the buffer
    char *head;       // pointer to head
    char *tail;       // pointer to tail
} circular_buffer;

void cb_init(circular_buffer *cb, size_t capacity, size_t sz);
void cb_free(circular_buffer *cb);
void cb_push_back(circular_buffer *cb, const char *item);
void cb_pop_front(circular_buffer *cb, char *item);
void cb_peek(circular_buffer *cb, char *item);

#endif
