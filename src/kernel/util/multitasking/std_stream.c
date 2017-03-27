#include "std_stream.h"

int std_stream_push(task_t* task, char* buf, int len) {
	for (int i = 0; i < len; i++) {
		std_stream_pushc(task, buf[i]);
	}
	return len;
}

int std_stream_pushc(task_t* task, char ch) {
	circular_buffer* buf = task->std_stream->buf;
	if (buf->count >= buf->capacity) {
		//TODO block until free space
		//ASSERT(0, "std_stream_push needs to block");
		return -1;
	}
	cb_push_back(buf, &ch);
	return 0;
}

int std_stream_pop(task_t* task, char* buf, int len) {
	memset(buf, 0, len);
	for (int i = 0; i < len; i++) {
		buf[i] = std_stream_popc(task);
	}
	return len;
}

char std_stream_popc(task_t* task) {
	circular_buffer* buf = task->std_stream->buf;
	if (!buf->count) {
		//TODO block until we have items to pop
		//ASSERT(0, "std_stream_popc needs to block");
		block_task(task, KB_WAIT);
		//return -1;
	}
	char ch;
	cb_pop_front(buf, &ch);
	return ch;
}

std_stream* std_stream_create() {
	std_stream* st = kmalloc(sizeof(std_stream));
	cb_init(st->buf, 16, sizeof(char));
	return st;
}

void std_stream_destroy(task_t* task) {
	cb_free(task->std_stream->buf);
	kfree(task->std_stream->buf);
	kfree(task->std_stream);
}

