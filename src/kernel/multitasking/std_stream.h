#ifndef STD_STREAM_H
#define STD_STREAM_H

#include <std/circular_buffer.h>

typedef struct std_stream {
	circular_buffer* buf;
} std_stream_t;

std_stream_t* std_stream_create();
void std_stream_destroy(std_stream_t*);

int std_stream_push(std_stream_t* stream, char* buf, int len);
int std_stream_pushchar(std_stream_t* stream, char ch);

int std_stream_pop(std_stream_t* stream, char* buf, int len);
char std_stream_popchar(std_stream_t* stream);

#endif
