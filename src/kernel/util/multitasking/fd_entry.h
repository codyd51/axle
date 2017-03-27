#ifndef FD_ENTRY_H
#define FD_ENTRY_H

typedef enum descriptor_type {
	STD_TYPE = 0,
	FILE_TYPE,
	PIPE_TYPE,
} descriptor_type;

typedef struct fd_entry {
	descriptor_type type;
	void* payload;
} fd_entry;

#endif
