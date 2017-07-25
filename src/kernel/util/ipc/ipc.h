#ifndef IPC_H
#define IPC_H

#include <stdint.h>

typedef struct ipc_state {
	char* region_start;
	char* region_end;
	char* next_unused_addr;
	char* kernel_addr;
} ipc_state_t;

int ipc_send(char* data, uint32_t size, uint32_t dest_pid, char** destination);

#endif
