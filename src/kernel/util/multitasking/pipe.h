#ifndef PIPE_H
#define PIPE_H

#include <std/circular_buffer.h>
#include <std/array_m.h>

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

	//list of PIDs referencing this pipe
	array_m* pids;
} pipe_t;

typedef struct pipe_block_info {
	pipe_t* pipe;
	int free_bytes_needed;
} pipe_block_info;

int pipe(int pipefd[2]);

int pipe_read(int fd, char* buf, int count);
int pipe_write(int fd, char* buf, int count);
int pipe_close(int fd);

#endif
