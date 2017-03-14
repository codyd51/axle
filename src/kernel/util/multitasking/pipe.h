#ifndef PIPE_H
#define PIPE_H

#include "circular_buffer.h"

typedef enum PIPE_DIR {
	READ = 0,
	WRITE,
} PIPE_DIR;

typedef struct pipe_t {
	//is this a read or write direction pipe?
	PIPE_DIR dir;
	//file descriptor associated with this pipe
	int fd;

	//backing buffer
	circular_buffer* cb;
} pipe_t;

int pipe(int pipefd[2]);

int pipe_read(int fd, char* buf, int count);
int pipe_write(int fd, char* buf, int count);

#endif
