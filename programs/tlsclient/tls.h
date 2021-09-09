#ifndef TLS_H
#define TLS_H

#include <stdint.h>

typedef enum tls_conn_state {
	TLS_INIT = 0,
	TLS_CLIENT_HELLO_SENT = 1,
	TLS_SERVER_HELLO_RECV = 2,
	TLS_SERVER_CERTIFICATE_RECV = 3,
} tls_conn_state_t;

typedef struct tls_conn {
	uint32_t tcp_conn_desc;
	uint8_t current_chunk[1024];
	uint32_t current_chunk_size;
	uint32_t read_off;
	uint32_t previously_read_byte_count;
	tls_conn_state_t state;
} tls_conn_t;

void tls_init(uint32_t tcp_conn_desc);
void* tls_read(tls_conn_t* state, uint32_t len);
uint8_t tls_read_byte(tls_conn_t* state);

#endif
