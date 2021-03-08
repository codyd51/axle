#ifndef NET_TCP_H
#define NET_TCP_H

#include <stdint.h>
#include "net.h"

typedef struct tcp_header {
	uint16_t source_port;
	uint16_t dest_port;

	uint32_t sequence_number;
	uint32_t acknowledgement_number;

	uint16_t header_length:4;
	uint16_t reserved:6;
	uint16_t urgent:1;
	uint16_t ack:1;
	uint16_t push:1;
	uint16_t reset:1;
	uint16_t synchronize:1;
	uint16_t finish:1;

	uint16_t window;
	uint16_t checksum;
	uint16_t urgent_pointer;

	// TODO(PT): What data size is this?
	uint16_t options[];
} __attribute((packed)) tcp_header_t;


#endif