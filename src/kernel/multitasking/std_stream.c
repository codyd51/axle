#include "std_stream.h"
#include <kernel/util/unistd/write.h>
#include <std/kheap.h>
#include <kernel/multitasking/tasks/task_small.h>

int std_stream_push(std_stream_t* stream, char* buf, int len) {
	for (int i = 0; i < len; i++) {
		std_stream_pushchar(stream, buf[i]);
	}
	return len;
}

int std_stream_pushchar(std_stream_t* stream, char ch) {
	if (!stream) return;
	//printf("std_stream_pushchar 0x%08x, %c\n", stream, ch);
	circular_buffer* buf = stream->buf;
	if (buf->count >= buf->capacity) {
		//TODO block until free space
		ASSERT(0, "std_stream_pushchar must block (full buf)");
		return -1;
	}
	cb_push_back(buf, &ch);

	/*
	//if an xterm is active, push this stdin
	Window* term = xterm_get();
	if (term) {
		std_write(task, 1, &ch, 1);
	}
	*/

	return 0;
}

int std_stream_pop(std_stream_t* stream, char* buf, int len) {
	memset(buf, 0, len);
	for (int i = 0; i < len; i++) {
		buf[i] = std_stream_popchar(stream);
	}
	return len;
}

char std_stream_popchar(std_stream_t* stream) {
	if (!stream) panic("bad stream");
	circular_buffer* buf = stream->buf;
	if (!buf->count) {
		task_small_t* task_to_block = tasking_get_current_task();
		tasking_block_task(task_to_block, KB_WAIT);
		assert(buf->count > 0, "Stdio buffer was still empty after blocking for input");
	}
	char ch;
	cb_pop_front(buf, &ch);
	return ch;
}

std_stream_t* std_stream_create() {
	std_stream_t* st = kmalloc(sizeof(std_stream_t));
	memset(st, 0, sizeof(std_stream_t));
	cb_init(st->buf, 16, sizeof(char));
	return st;
}

void std_stream_destroy(std_stream_t* stream) {
	cb_free(stream->buf);
	kfree(stream->buf);
	kfree(stream);
}

