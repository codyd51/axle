#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <stdlibadd/assert.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

#include <awm/awm.h>

#include <libnet/libnet.h>
#include <libgui/libgui.h>
#include <libport/libport.h>

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

typedef struct tls_conn {
	uint32_t tcp_conn_desc;
	uint8_t current_chunk[1024];
	uint32_t current_chunk_size;
	uint32_t read_off;
	uint32_t previously_read_byte_count;
} tls_conn_t;

static char _tls_read_char(tls_conn_t* state) {
	// have we yet to read our first chunk, or are out of data within this chunk?
	if (state->current_chunk_size == 0 || state->read_off >= state->current_chunk_size) {
		printf("lexer fetching more stream data, read_off %d curr_chunk_size %d\n", state->read_off, state->current_chunk_size);
		state->previously_read_byte_count += state->current_chunk_size;
		state->current_chunk_size = net_tcp_conn_read(state->tcp_conn_desc, state->current_chunk, sizeof(state->current_chunk));
		printf("lexer recv %d bytes from stream\n", state->current_chunk_size);
		state->read_off = 0;
	}
	assert(state->current_chunk_size > 0, "expected stream data to be available");

	return state->current_chunk[state->read_off++];
}

static void* _tls_read(tls_conn_t* state, uint32_t len) {
	char buf[len];
	uint32_t i = 0;
	for (; i < len; i++) {
		char c = _tls_read_char(state);
		buf[i] = c;
	}
	char* copy = calloc(1, len);
	memcpy(copy, buf, i);
	return copy;
}

static uint32_t _read_24bit_value(tls_conn_t* state) {
	uint8_t* buf = _tls_read(state, 3 * sizeof(uint8_t));
	uint8_t* buf_head = buf;
	uint32_t val = (*buf++ << 16) | (*buf++ << 8) | (*buf);
	free(buf_head);
	return val;
}

static void _url_bar_received_input(text_input_t* text_input, char ch) {
	char* domain_name = "google.com";
	uint32_t domain_name_len = strlen(domain_name);
	printf("TLS: Performing DNS lookup of %.*s\n", domain_name_len, domain_name);
	uint8_t out_ipv4[IPv4_ADDR_SIZE];
	net_get_ipv4_of_domain_name(domain_name, domain_name_len, out_ipv4);
	char buf[64];
	format_ipv4_address__buf(buf, sizeof(buf), out_ipv4);
	printf("TLS: IPv4 address of %s: %s\n", domain_name, buf);

	uint32_t port = net_find_free_port();
	uint32_t dest_port = 443;
	uint32_t conn = net_tcp_conn_init(port, dest_port, out_ipv4);
	printf("TCP: Conn descriptor %d\n", conn);

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

	net_tcp_conn_send(conn, message_buf, len);

	// Start parsing TLS data from the remote end
	tls_conn_t* state = calloc(1, sizeof(tls_conn_t));
	state->tcp_conn_desc = conn;

	tls_plaintext_t* server_hello_header = _tls_read(state, sizeof(tls_plaintext_t));
	printf("server hello content type %d\n", server_hello_header->content_type);
	printf("server hello proto_vers %d.%d\n", server_hello_header->proto_version.major, server_hello_header->proto_version.minor);
	printf("server hello data len %d\n", ntohs(server_hello_header->data_len));
	free(server_hello_header);

	uint8_t* server_hello_handshake_header_msg_type = _tls_read(state, sizeof(uint8_t));
	printf("server_hello handshake type: %d\n", *server_hello_handshake_header_msg_type);
	assert(*server_hello_handshake_header_msg_type == TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_HELLO, "Expected server hello");
	printf("0x%08x\n", server_hello_handshake_header_msg_type);
	free(server_hello_handshake_header_msg_type);
	printf("after free\n");

	uint32_t server_hello_len = _read_24bit_value(state);
	printf("server_hello handshake length2: %d\n", server_hello_len);
	uint32_t server_hello_start = state->read_off;

	tls_handshake_msg_server_hello_start_t* server_hello_msg_start = _tls_read(state, sizeof(tls_handshake_msg_server_hello_start_t));
	printf("server hello proto_vers %d.%d\n", server_hello_msg_start->server_version.major, server_hello_msg_start->server_version.minor);
	printf("server hello session id length %d\n", server_hello_msg_start->session_id_length);
	uint32_t session_id_length = server_hello_msg_start->session_id_length;
	free(server_hello_msg_start);

	uint8_t* session_id = _tls_read(state, session_id_length);
	printf("session_id %02x %02x %02x %02x\n", session_id[0], session_id[1], session_id[2], session_id[3]);
	free(session_id);

	tls_handshake_msg_server_hello_end_t* server_hello_msg_end = _tls_read(state, sizeof(tls_handshake_msg_server_hello_end_t));
	uint8_t* b = (uint8_t*)server_hello_msg_end;
	printf("server hello cipher_suite %02x\n", ntohs(server_hello_msg_end->cipher_suite));
	printf("server hello compression %02x\n", ntohs(server_hello_msg_end->compression_method));
	free(server_hello_msg_start);

	uint32_t server_hello_end = state->read_off;
	printf("Leftover %d\n", server_hello_end - server_hello_start);

	tls_plaintext_t* server_cert = _tls_read(state, sizeof(tls_plaintext_t));
	printf("server cert content type %d\n", server_cert->content_type);
	printf("server cert proto_vers %d.%d\n", server_cert->proto_version.major, server_cert->proto_version.minor);
	printf("server cert data len %d\n", ntohs(server_cert->data_len));
	free(server_cert);

	uint8_t* server_cert_handshake_header_msg_type = _tls_read(state, sizeof(uint8_t));
	printf("server_hello handshake type: %d\n", *server_hello_handshake_header_msg_type);
	assert(*server_hello_handshake_header_msg_type == TLS_HANDSHAKE_MESSAGE_TYPE_SERVER_CERTIFICATE, "Expected server certificate");
	free(server_hello_handshake_header_msg_type);

	uint32_t server_cert_len = _read_24bit_value(state);
	printf("server_cert handshake length: %d\n", server_cert_len);

	while (1) {
		char buf[2048];
		uint32_t byte_count = net_tcp_conn_read(conn, buf, sizeof(buf));
		printf("Read %d bytes from socket\n", byte_count);
	}
}

static Rect _url_bar_sizer(text_input_t* text_input, Size window_size) {
	Size search_bar_size = size_make(window_size.width, 60);
	return rect_make(point_zero(), search_bar_size);
}

static Rect _render_box_sizer(text_view_t* text_view, Size window_size) {
	// TODO(PT): Pull in search bar height instead of hard-coding it
	uint32_t search_bar_height = 60;
	return rect_make(
		point_make(0, search_bar_height),
		size_make(
			window_size.width, 
			window_size.height - search_bar_height
		)
	);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.tlsclient");
	printf("TLS client running\n");

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("TLS Browser", 800, 800);
	Size window_size = window->size;

	Size search_bar_size = size_make(window_size.width, 60);
	text_input_t* url_input = gui_text_input_create(
		window,
		rect_make(point_zero(), search_bar_size), 
		color_white(),
		(gui_window_resized_cb_t)_url_bar_sizer
	);
	url_input->text_box->font_size = size_make(12, 20);
	url_input->text_entry_cb = (text_input_text_entry_cb_t)_url_bar_received_input;
	gui_text_input_set_prompt(url_input, "Type any character to try TLS");

	Rect render_box_frame = rect_make(
		point_make(0, search_bar_size.height),
		size_make(
			window_size.width, 
			window_size.height - search_bar_size.height
		)
	);
	text_view_t* render_box = gui_text_view_create(
		window, 
		render_box_frame,
		color_white(),
		(gui_window_resized_cb_t)_render_box_sizer
	);

	// Enter the event loop forever
	gui_enter_event_loop(window);

	return 0;
}
