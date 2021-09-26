#ifndef FD_ENTRY_H
#define FD_ENTRY_H

typedef enum descriptor_type {
	STD_TYPE = 0,
	FILE_TYPE,
	PIPE_TYPE,
} descriptor_type_t;

typedef struct fd_entry {
	descriptor_type_t type;
	void* payload;
} fd_entry_t;

#endif
