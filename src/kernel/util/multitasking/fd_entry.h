#ifndef FD_ENTRY_H
#define FD_ENTRY_H

typedef enum descriptor_type {
	STDIN_TYPE = 0,
	STDOUT_TYPE,
	STDERR_TYPE,
	FILE_TYPE,
	PIPE_TYPE,
} descriptor_type;

typedef struct fd_entry {
	descriptor_type type;
	void* payload;
} fd_entry;

#endif
