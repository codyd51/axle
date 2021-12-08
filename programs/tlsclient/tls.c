#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <libutils/assert.h>

#include <libnet/libnet.h>
#include <libport/libport.h>

#include "tls.h"
#include "asn1.h"

typedef struct tls_protocol_version {
	uint8_t major;
	uint8_t minor;
} __attribute__((packed)) tls_protocol_version_t;

typedef struct tls_plaintext {
	uint8_t content_type;
	tls_protocol_version_t proto_version;
	uint16_t data_len;
	uint8_t data[];
} __attribute__((packed)) tls_plaintext_t;

#define TLS_CONTENT_TYPE_HANDSHAKE 0x16

typedef struct tls_handshake_header {
	uint8_t msg_type;
	uint32_t length:24;
	uint8_t data[];
} __attribute__((packed)) tls_handshake_header_t;

#define TLS_HANDSHAKE_MESSAGE_TYPE_CLIENT_HELLO 0x01
#define TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_HELLO 0x02
#define TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_CERTIFICATE 0x0B

typedef struct tls_random {
	uint8_t data[32];
}  __attribute__((packed)) tls_random_t;

typedef struct tls_cipher_suites {
	// Not Implemented
}  __attribute__((packed)) tls_cipher_suites_t;

typedef struct tls_single_cipher_suite {
	uint16_t len;
	uint16_t cipher_suite;
}  __attribute__((packed)) tls_single_cipher_suite_t;

typedef struct tls_single_compression_method {
	uint8_t len;
	uint8_t compression_method;
}  __attribute__((packed)) tls_single_compression_method_t;

typedef struct tls_handshake_msg_client_hello {
	tls_protocol_version_t client_version;
	tls_random_t client_random;
	uint8_t session_id;
	tls_single_cipher_suite_t cipher_suites;
	tls_single_compression_method_t compression_methods;
}  __attribute__((packed)) tls_handshake_msg_client_hello_t;

typedef struct tls_handshake_msg_server_hello_start {
	tls_protocol_version_t server_version;
	tls_random_t server_random;
	uint8_t session_id_length;
} __attribute__((packed)) tls_handshake_msg_server_hello_start_t;

// session_id_length bytes follow

typedef struct tls_handshake_msg_server_hello_end {
	uint16_t cipher_suite;
	uint8_t compression_method;
}  __attribute__((packed)) tls_handshake_msg_server_hello_end_t;

#define TLS_RSA_WITH_AES_128_GCM_SHA256 0x009c
#define TLS_RSA_WITH_AES_128_CBC_SHA	0x002F
#define TLS_NO_COMPRESSION 0x00

uint32_t stub_read(uint32_t conn_descriptor, uint8_t* buf, uint32_t len) {
	/*
	char* data = "\x16\x03\x03\x00\x4a\x02\x00\x00\x46\x03" \
"\x03\x60\x82\xd2\x20\x5c\x56\x15\xdb\x76\x7f\x38\xa6\xbb\x60\xff" \
"\x11\x0c\x85\xd8\x34\x83\xe7\x8d\xc6\x44\x4f\x57\x4e\x47\x52\x44" \
"\x01\x20\x33\xab\xc8\x9d\x83\x68\xb6\x4e\xa9\x7d\x65\xfe\x5a\x12" \
"\x97\x9e\xd0\x45\x99\xde\xb2\xd5\x7e\x1f\x16\x81\xb5\x3a\xc6\x3a" \
"\xb5\x60\x00\x2f\x00\x16\x03\x03\x0e\xc5\x0b\x00\x0e\xc1\x00\x0e" \
"\xbe\x00\x0a\x6a\x30\x82\x0a\x66\x30\x82\x09\x4e\xa0\x03\x02\x01" \
"\x02\x02\x10\x06\xfb\x72\xd1\x20\x27\x3e\xb9\x03\x00\x00\x00\x00" \
"\xcb\xd6\x05\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b" \
"\x05\x00\x30\x42\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55" \
"\x53\x31\x1e\x30\x1c\x06\x03\x55\x04\x0a\x13\x15\x47\x6f\x6f\x67" \
"\x6c\x65\x20\x54\x72\x75\x73\x74\x20\x53\x65\x72\x76\x69\x63\x65" \
"\x73\x31\x13\x30\x11\x06\x03\x55\x04\x03\x13\x0a\x47\x54\x53\x20" \
"\x43\x41\x20\x31\x4f\x31\x30\x1e\x17\x0d\x32\x31\x30\x33\x32\x33" \
"\x30\x38\x30\x34\x35\x36\x5a\x17\x0d\x32\x31\x30\x36\x31\x35\x30" \
"\x38\x30\x34\x35\x35\x5a\x30\x66\x31\x0b\x30\x09\x06\x03\x55\x04" \
"\x06\x13\x02\x55\x53\x31\x13\x30\x11\x06\x03\x55\x04\x08\x13\x0a" \
"\x43\x61\x6c\x69\x66\x6f\x72\x6e\x69\x61\x31\x16\x30\x14\x06\x03" \
"\x55\x04\x07\x13\x0d\x4d\x6f\x75\x6e\x74\x61\x69\x6e\x20\x56\x69" \
"\x65\x77\x31\x13\x30\x11\x06\x03\x55\x04\x0a\x13\x0a\x47\x6f\x6f" \
"\x67\x6c\x65\x20\x4c\x4c\x43\x31\x15\x30\x13\x06\x03\x55\x04\x03" \
"\x0c\x0c\x2a\x2e\x67\x6f\x6f\x67\x6c\x65\x2e\x63\x6f\x6d\x30\x82" \
"\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05" \
"\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xab" \
"\x17\xae\xf5\x51\xc8\x03\x24\xda\xcd\x76\x9f\x9f\x93\xb8\xe3\x6c" \
"\xd4\xe7\x8c\x8c\xe1\x26\xab\xc0\x09\x6c\x83\xa1\xcf\x5b\x0d\xdc" \
"\x0c\xfb\xd7\x28\x59\xb4\x89\x6c\x3b\x67\xd9\x94\x86\xa7\x96\xb3" \
"\xb8\x6b\x9c\xa9\x0a\x06\x49\x12\xe1\xf7\x7c\x27\xf0\xdd\xef\xbb" \
"\x20\xb9\xbb\xfe\xcf\x72\xa7\xc9\x6f\x1b\xec\x70\xea\xf1\x0e\xaa" \
"\xde\x5e\xdf\x49\x67\xb1\xce\xcc\xc9\x4a\x4c\x09\xee\x03\x5c\x3a" \
"\x52\x44\x2f\xb4\x19\x44\xe1\x88\xda\x3d\xba\xa0\x71\x1c\x29\x63" \
"\x3b\x80\xe6\xe2\x01\xea\x27\xf9\xb6\x3f\xb0\xdb\x90\x4a\xfc\x1b" \
"\x21\x91\x68\x6e\x9b\x2a\x34\x29\xd7\x25\xa2\x6b\xa9\xcd\x8b\x06" \
"\xe7\xe1\xd4\x80\xbf\x13\xab\xa4\x81\xfc\xcd\xbb\x81\xbe";
*/
char* data = "\x16\x03\x03\x00\x4a\x02\x00\x00\x46\x03" \
"\x03\x60\x82\xba\xda\x58\x9b\xa6\xf2\x63\x91\x44\xa1\x0c\xfb\x67" \
"\xc2\x8a\xc2\xe0\x7c\x6b\x83\x66\xa2\x44\x4f\x57\x4e\x47\x52\x44" \
"\x01\x20\x0a\x82\x75\x24\x49\x35\x2b\x92\xe6\xbe\x27\xba\xa0\x92" \
"\xbc\xab\xfc\xcd\x3c\xf8\xb8\xd5\x8d\x1a\x1d\x97\x48\xcb\x81\xbe" \
"\x7a\x2b\x00\x2f\x00\x16\x03\x03\x0e\xc5\x0b\x00\x0e\xc1\x00\x0e" \
"\xbe\x00\x0a\x6a\x30\x82\x0a\x66\x30\x82\x09\x4e\xa0\x03\x02\x01" \
"\x02\x02\x10\x06\xfb\x72\xd1\x20\x27\x3e\xb9\x03\x00\x00\x00\x00" \
"\xcb\xd6\x05\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x0b" \
"\x05\x00\x30\x42\x31\x0b\x30\x09\x06\x03\x55\x04\x06\x13\x02\x55" \
"\x53\x31\x1e\x30\x1c\x06\x03\x55\x04\x0a\x13\x15\x47\x6f\x6f\x67" \
"\x6c\x65\x20\x54\x72\x75\x73\x74\x20\x53\x65\x72\x76\x69\x63\x65" \
"\x73\x31\x13\x30\x11\x06\x03\x55\x04\x03\x13\x0a\x47\x54\x53\x20" \
"\x43\x41\x20\x31\x4f\x31\x30\x1e\x17\x0d\x32\x31\x30\x33\x32\x33" \
"\x30\x38\x30\x34\x35\x36\x5a\x17\x0d\x32\x31\x30\x36\x31\x35\x30" \
"\x38\x30\x34\x35\x35\x5a\x30\x66\x31\x0b\x30\x09\x06\x03\x55\x04" \
"\x06\x13\x02\x55\x53\x31\x13\x30\x11\x06\x03\x55\x04\x08\x13\x0a" \
"\x43\x61\x6c\x69\x66\x6f\x72\x6e\x69\x61\x31\x16\x30\x14\x06\x03" \
"\x55\x04\x07\x13\x0d\x4d\x6f\x75\x6e\x74\x61\x69\x6e\x20\x56\x69" \
"\x65\x77\x31\x13\x30\x11\x06\x03\x55\x04\x0a\x13\x0a\x47\x6f\x6f" \
"\x67\x6c\x65\x20\x4c\x4c\x43\x31\x15\x30\x13\x06\x03\x55\x04\x03" \
"\x0c\x0c\x2a\x2e\x67\x6f\x6f\x67\x6c\x65\x2e\x63\x6f\x6d\x30\x82" \
"\x01\x22\x30\x0d\x06\x09\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x05" \
"\x00\x03\x82\x01\x0f\x00\x30\x82\x01\x0a\x02\x82\x01\x01\x00\xab" \
"\x17\xae\xf5\x51\xc8\x03\x24\xda\xcd\x76\x9f\x9f\x93\xb8\xe3\x6c" \
"\xd4\xe7\x8c\x8c\xe1\x26\xab\xc0\x09\x6c\x83\xa1\xcf\x5b\x0d\xdc" \
"\x0c\xfb\xd7\x28\x59\xb4\x89\x6c\x3b\x67\xd9\x94\x86\xa7\x96\xb3" \
"\xb8\x6b\x9c\xa9\x0a\x06\x49\x12\xe1\xf7\x7c\x27\xf0\xdd\xef\xbb" \
"\x20\xb9\xbb\xfe\xcf\x72\xa7\xc9\x6f\x1b\xec\x70\xea\xf1\x0e\xaa" \
"\xde\x5e\xdf\x49\x67\xb1\xce\xcc\xc9\x4a\x4c\x09\xee\x03\x5c\x3a" \
"\x52\x44\x2f\xb4\x19\x44\xe1\x88\xda\x3d\xba\xa0\x71\x1c\x29\x63" \
"\x3b\x80\xe6\xe2\x01\xea\x27\xf9\xb6\x3f\xb0\xdb\x90\x4a\xfc\x1b" \
"\x21\x91\x68\x6e\x9b\x2a\x34\x29\xd7\x25\xa2\x6b\xa9\xcd\x8b\x06" \
"\xe7\xe1\xd4\x80\xbf\x13\xab\xa4\x81\xfc\xcd\xbb\x81\xbe";

	memcpy(buf, data, 536);
	return 536;
}

uint8_t tls_read_byte(tls_conn_t* state) {
	// have we yet to read our first chunk, or are out of data within this chunk?
	if (state->current_chunk_size == 0 || state->read_off >= state->current_chunk_size) {
		printf("lexer fetching more stream data, read_off %d curr_chunk_size %d\n", state->read_off, state->current_chunk_size);
		state->previously_read_byte_count += state->current_chunk_size;
		//state->current_chunk_size = net_tcp_conn_read(state->tcp_conn_desc, state->current_chunk, sizeof(state->current_chunk));
		state->current_chunk_size = stub_read(state->tcp_conn_desc, state->current_chunk, sizeof(state->current_chunk));
		printf("lexer recv %d bytes from stream\n", state->current_chunk_size);
		state->read_off = 0;
	}
	assert(state->current_chunk_size > 0, "expected stream data to be available");

	return state->current_chunk[state->read_off++];
}

void* tls_read(tls_conn_t* state, uint32_t len) {
	char buf[len];
	uint32_t i = 0;
	for (; i < len; i++) {
		char c = tls_read_byte(state);
		buf[i] = c;
	}
	char* copy = calloc(1, len);
	memcpy(copy, buf, i);
	return copy;
}

static uint32_t _read_24bit_value(tls_conn_t* state) {
	uint8_t* buf = tls_read(state, 3 * sizeof(uint8_t));
	uint8_t* buf_head = buf;
	uint32_t val = (*buf++ << 16) | (*buf++ << 8) | (*buf);
	free(buf_head);
	return val;
}

static void _tls_send_client_hello(tls_conn_t* state) {
	uint8_t* message_buf = calloc(1, 1024);
	tls_plaintext_t* header = (tls_plaintext_t*)message_buf;
	header->content_type = TLS_CONTENT_TYPE_HANDSHAKE;
	// "3.1" is TLS 1.0
	header->proto_version = (tls_protocol_version_t){.major = 3, .minor = 1};

	tls_handshake_header_t* handshake_header = (tls_handshake_header_t*)header->data;
	handshake_header->msg_type = TLS_HANDSHAKE_MESSAGE_TYPE_CLIENT_HELLO;

	tls_handshake_msg_client_hello_t* client_hello = (tls_handshake_msg_client_hello_t*)handshake_header->data;
	client_hello->client_version = (tls_protocol_version_t){.major = 3, .minor = 3};
	client_hello->client_random = (tls_random_t){0x60, 0x77, 0x6f, 0xa7, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00, 0xaa};
	client_hello->session_id = 0x00;
	tls_single_cipher_suite_t* cipher_suite = &client_hello->cipher_suites;
	cipher_suite->len = htons(sizeof(uint16_t));
	cipher_suite->cipher_suite = htons(TLS_RSA_WITH_AES_128_CBC_SHA);
	tls_single_compression_method_t* compression_method = &client_hello->compression_methods;
	compression_method->len = sizeof(uint8_t);
	compression_method->compression_method = TLS_NO_COMPRESSION;

	uint8_t* end = (uint8_t*)compression_method + sizeof(tls_single_compression_method_t);

	// Write the 24-bit handshake length
	// https://stackoverflow.com/questions/3965873/how-to-write-a-24-bit-message-after-reading-from-a-4-byte-integer-on-a-big-endia
	uint32_t handshake_length = end - (uint8_t*)&handshake_header->data;
	uint8_t* len_ptr = (uint8_t*)handshake_header + sizeof(uint8_t);
	*len_ptr++ = (handshake_length >> 16) & 0xFF;
	*len_ptr++ = (handshake_length >> 8)  & 0xFF;
	*len_ptr++ = (handshake_length)       & 0xFF;

	header->data_len = htons(end - (uint8_t*)&header->data);
	uint32_t len = end - message_buf;

	//net_tcp_conn_send(conn, message_buf, len);
	free(message_buf);
}

static void _tls_process_server_hello(tls_conn_t* state) {
	assert(state->state == TLS_CLIENT_HELLO_SENT, "Expected CLIENT_HELLO -> SERVER_HELLO");
	state->state = TLS_SERVER_HELLO_RECV;

	tls_plaintext_t* server_hello_header = tls_read(state, sizeof(tls_plaintext_t));
	printf("server hello content type %d\n", server_hello_header->content_type);
	printf("server hello proto_vers %d.%d\n", server_hello_header->proto_version.major, server_hello_header->proto_version.minor);
	printf("server hello data len %d\n", ntohs(server_hello_header->data_len));
	free(server_hello_header);

	uint8_t* server_hello_handshake_header_msg_type = tls_read(state, sizeof(uint8_t));
	printf("server_hello handshake type: %d\n", *server_hello_handshake_header_msg_type);
	assert(*server_hello_handshake_header_msg_type == TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_HELLO, "Expected server hello");
	free(server_hello_handshake_header_msg_type);

	uint32_t server_hello_len = _read_24bit_value(state);
	printf("server_hello handshake length2: %d\n", server_hello_len);
	uint32_t server_hello_start = state->read_off;

	tls_handshake_msg_server_hello_start_t* server_hello_msg_start = tls_read(state, sizeof(tls_handshake_msg_server_hello_start_t));
	printf("server hello proto_vers %d.%d\n", server_hello_msg_start->server_version.major, server_hello_msg_start->server_version.minor);
	printf("server hello session id length %d\n", server_hello_msg_start->session_id_length);
	uint32_t session_id_length = server_hello_msg_start->session_id_length;
	free(server_hello_msg_start);

	uint8_t* session_id = tls_read(state, session_id_length);
	printf("session_id %02x %02x %02x %02x\n", session_id[0], session_id[1], session_id[2], session_id[3]);
	free(session_id);

	tls_handshake_msg_server_hello_end_t* server_hello_msg_end = tls_read(state, sizeof(tls_handshake_msg_server_hello_end_t));
	uint8_t* b = (uint8_t*)server_hello_msg_end;
	printf("server hello cipher_suite %02x\n", ntohs(server_hello_msg_end->cipher_suite));
	printf("server hello compression %02x\n", ntohs(server_hello_msg_end->compression_method));
	free(server_hello_msg_start);

	uint32_t server_hello_end = state->read_off;
	printf("Leftover %d\n", server_hello_end - server_hello_start);
}

static void _tls_process_server_certificate(tls_conn_t* state) {
	assert(state->state == TLS_SERVER_HELLO_RECV, "Expected SERVER_HELLO_RECV -> SERVER_CERTIFICATE_RECV");
	state->state = TLS_SERVER_CERTIFICATE_RECV;

	tls_plaintext_t* server_cert = tls_read(state, sizeof(tls_plaintext_t));
	printf("server cert content type %d\n", server_cert->content_type);
	printf("server cert proto_vers %d.%d\n", server_cert->proto_version.major, server_cert->proto_version.minor);
	printf("server cert data len %d\n", ntohs(server_cert->data_len));
	free(server_cert);

	uint8_t* server_cert_handshake_header_msg_type = tls_read(state, sizeof(uint8_t));
	printf("server_cert handshake type: %d\n", *server_cert_handshake_header_msg_type);
	assert(*server_cert_handshake_header_msg_type == TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_CERTIFICATE, "Expected server certificate");
	free(server_cert_handshake_header_msg_type);

	uint32_t server_cert_len = _read_24bit_value(state);
	printf("server_cert handshake length: %d\n", server_cert_len);

	uint32_t certificates_len = _read_24bit_value(state);
	printf("server_cert certificates len %d\n", certificates_len);

	uint32_t certificate_len = _read_24bit_value(state);
	printf("server_cert first certificate len %d\n", certificate_len);
	asn1_cert_parse(state, certificate_len);
}

void tls_init(uint32_t tcp_conn_desc) {
	tls_conn_t* state = calloc(1, sizeof(tls_conn_t));
	state->tcp_conn_desc = tcp_conn_desc;
	state->state = TLS_CLIENT_HELLO_SENT;

	//_tls_send_client_hello(state);
	// Start parsing TLS data from the remote end
	_tls_process_server_hello(state);
	_tls_process_server_certificate(state);

	while (1) {
		char buf[2048];
		uint32_t byte_count = net_tcp_conn_read(tcp_conn_desc, buf, sizeof(buf));
		printf("Read %d bytes from socket\n", byte_count);
	}
}
