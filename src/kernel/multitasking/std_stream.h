#ifndef STD_STREAM_H
#define STD_STREAM_H

#include <std/circular_buffer.h>

typedef struct std_stream {
	circular_buffer* buf;
} std_stream;

#include <kernel/util/multitasking/tasks/task.h>

std_stream* std_stream_create();
void std_stream_destroy(task_t* task);

int std_stream_push(task_t* task, char* buf, int len);
int std_stream_pushc(task_t* task, char ch);

int std_stream_pop(task_t* task, char* buf, int len);
char std_stream_popc(task_t* task);

#endif
