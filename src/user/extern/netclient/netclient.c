#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <stdlibadd/array.h>
#include <stdlibadd/assert.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

#include <awm/awm.h>

#include <libnet/libnet.h>

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

static ca_layer* window_layer_get(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send("com.axle.awm", AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);

	// And get back info about the window it made
	amc_message_t* receive_framebuf;
	amc_message_await("com.axle.awm", &receive_framebuf);
	uint32_t event = amc_msg_u32_get_word(receive_framebuf, 0);
	if (event != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}
	uint32_t framebuffer_addr = amc_msg_u32_get_word(receive_framebuf, 1);

	printf("Received framebuffer from awm: %d 0x%08x\n", event, framebuffer_addr);
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = malloc(sizeof(ca_layer));
	memset(dummy_layer, 0, sizeof(dummy_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	return dummy_layer;
}

typedef struct text_input {
	text_box_t* text_box;
	Rect frame;
	uint8_t* text;
	uint32_t len;
	uint32_t max_len;
} text_input_t;

typedef struct text_view {
	text_box_t* text_box;
	Rect frame;
} text_view_t;

typedef struct event_loop_state {
	Rect window_frame;
	ca_layer* window_layer;
	array_t* text_inputs;
	array_t* text_views;
} event_loop_state_t;

text_input_t* text_input_create(Rect frame, Color background_color) {
	text_input_t* text_input = calloc(1, sizeof(text_input_t));
	text_box_t* text_box = text_box_create(frame.size, background_color);
	text_input->text_box = text_box;
	text_input->frame = frame;

	uint32_t initial_bufsize = 64;
	text_input->text = calloc(1, initial_bufsize);
	text_input->max_len = initial_bufsize;
	return text_input;
}

void text_input_clear(text_input_t* ti) {
	text_box_clear(ti->text_box);
	memset(ti->text, 0, ti->max_len);
	ti->len = 0;
}

text_view_t* text_view_create(Rect frame, Color background_color) {
	text_view_t* text_view = calloc(1, sizeof(text_view_t));
	text_box_t* text_box = text_box_create(frame.size, background_color);
	text_view->text_box = text_box;
	text_view->frame = frame;
	return text_view;
}

typedef struct tcp_lexer {
	uint32_t tcp_conn_desc;
	uint8_t current_chunk[1024];
	uint32_t current_chunk_size;
	uint32_t read_off;

	char delimiters[64];
	uint32_t delimiters_count;

	// html info
	// todo(pt): replace with a hash map of headers?
	uint32_t content_length;
} tcp_lexer_t;

static char _peekchar(tcp_lexer_t* state) {
	// have we yet to read our first chunk, or are out of data within this chunk?
	if (state->current_chunk_size == 0 || state->read_off >= state->current_chunk_size) {
		printf("lexer fetching more stream data\n");
		state->current_chunk_size = net_tcp_conn_read(state->tcp_conn_desc, state->current_chunk, sizeof(state->current_chunk));
		printf("lexer recv %d bytes from stream\n", state->current_chunk_size);
		state->read_off = 0;
	}
	assert(state->current_chunk_size > 0, "expected stream data to be available");

	return state->current_chunk[state->read_off];
}

static char _getchar(tcp_lexer_t* state) {
	char ret = _peekchar(state);
	state->read_off += 1;
	return ret;
}

static bool _char_is_delimiter(tcp_lexer_t* state, char ch) {
	for (uint32_t i = 0; i < state->delimiters_count; i++) {
		if (ch == state->delimiters[i]) {
			return true;
		}
	}
	return false;
}

static char* _gettok(tcp_lexer_t* state, uint32_t* out_len) {
	char buf[128];
	uint32_t i = 0;
	for (; i < sizeof(buf); i++) {
		char peek = _peekchar(state);
		bool is_delimiter = _char_is_delimiter(state, peek);
		if (is_delimiter) {
			if (i > 0) {
				// if this is a multi-character token, break if we encounter a delimiter
				break;
			}
			else {
				// 1-character delimiter token
				// ensure `i` is incremented to reflect a token, even though we break
				buf[i++] = _getchar(state);
				break;
			}
		}

		// normal token
		buf[i] = _getchar(state);
	}
	*out_len = i;
	return strndup(buf, i);
}

static bool _match(tcp_lexer_t* state, char* expected) {
	char buf[256];
	uint32_t buf_ptr = 0;
	uint32_t expected_len = strlen(expected);
	for (; buf_ptr < sizeof(buf);) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len);
		strncpy(buf + buf_ptr, next_tok, tok_len);
		buf_ptr += tok_len;
		free(next_tok);

		if (buf_ptr >= expected_len) {
			if (!strncmp(buf, expected, expected_len)) {
				return true;
			}
			return false;
		}
	}
	return false;
}

static bool _check_stream(tcp_lexer_t* state, char* expected) {
	char buf[256];
	uint32_t buf_ptr = 0;
	uint32_t expected_len = strlen(expected);

	if (state->read_off + expected_len > state->current_chunk_size) {
		printf("state->read_off %d expected_len %d, current_chunk_size %d\n", state->read_off, expected_len, state->current_chunk_size);
		assert(0, "cannot peek stream: need to fetch more data");
		return false;
	}

	char* stream_ptr = state->current_chunk + state->read_off;
	return !strncmp(stream_ptr, expected, expected_len);
}

static bool _check_newline(tcp_lexer_t* state) {
	return _check_stream(state, "\r\n") || _check_stream(state, "\n");
}

static bool _match_newline(tcp_lexer_t* state, bool* out_is_crlf) {
	*out_is_crlf = false;

	if (_check_stream(state, "\r\n")) {
		*out_is_crlf = true;
		return _match(state, "\r\n");
	}
	else if (_check_stream(state, "\n")) {
		return _match(state, "\n");
	}
	return false;
}

static bool _check_crlf(tcp_lexer_t* state) {
	return _check_stream(state, "\r\n");
}

bool str_ends_with(const char *s, const char *t) {
	// https://codereview.stackexchange.com/questions/54722/determine-if-one-string-occurs-at-the-end-of-another/54724
    size_t ls = strlen(s);
    size_t lt = strlen(t);
	// Check if t can fit in s
    if (ls >= lt) {
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (ls - lt), lt));
    }
	// t was longer than s
    return 0;
}

static char* _get_token_sequence_delimited_by(tcp_lexer_t* state, char* delim, uint32_t* out_len, bool consume_delim) {
	char buf[256];
	uint32_t buf_ptr = 0;
	for (; buf_ptr < sizeof(buf);) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len);
		if (!strncmp(next_tok, delim, strlen(delim))) {
			free(next_tok);
			break;
		}
		strncpy(buf + buf_ptr, next_tok, tok_len);
		buf_ptr += tok_len;
		free(next_tok);

		// In case the delimiter was split up into multiple tokens,
		// check the last bit of the string to see if it matches the delimiter
		if (str_ends_with(buf, delim)) {
			// Trim the delimiter from the string
			buf_ptr -= strlen(delim);
			break;
		}
	}

	// If we should not consume the delimiter, rewind the stream
	if (!consume_delim) {
		state->read_off -= strlen(delim);
	}

	*out_len = buf_ptr;
	return strndup(buf, buf_ptr);
}

tcp_lexer_t* _lexer_create(void) {
	tcp_lexer_t* lexer = calloc(1, sizeof(tcp_lexer_t));
	char delimiters[] = {' ', '<', '>', '\r', '\n'};
	uint32_t delims_len = sizeof(delimiters) / sizeof(delimiters[0]);
	memcpy(lexer->delimiters, delimiters, delims_len);
	lexer->delimiters_count = delims_len;
	return lexer;
}

void _lexer_free(tcp_lexer_t* lexer) {
	free(lexer);
}

typedef struct html_tag {
	char* name;
	char* attrs;
} html_tag_t;

static html_tag_t* _parse_html_tag(tcp_lexer_t* state) {
	html_tag_t* tag = calloc(1, sizeof(html_tag_t));
	if (!_match(state, "<")) {
		printf("Failed to match open tag %c\n", _peekchar(state));
		return NULL;
	}
	uint32_t out_len = 0;
	tag->name = _gettok(state, &out_len);
	// Close tag or more attributes to follow?
	if (_peekchar(state) == ' ') {
		_match(state, " ");
		tag->attrs = _get_token_sequence_delimited_by(state, ">",  &out_len, true);

	}
	else {
		// No attributes - close the tag
		if (!_match(state, ">")) {
			printf("Failed to match close tag\n");
			free(tag->name);
			free(tag);
			return NULL;
		}
	}
	return tag;
}

static void html_tag_free(html_tag_t* t) {
	if (t->name) {
		free(t->name);
	}
	if (t->attrs) {
		free(t->attrs);
	}
	free(t);
}

static void _read_html_response(uint32_t conn_desc) {
	tcp_lexer_t* lexer = _lexer_create();

	uint32_t out_len = 0;
	char* http_version = _get_token_sequence_delimited_by(lexer, " ", &out_len, true);
	printf("HTTP version: %.*s\n", out_len, http_version);
	free(http_version);

	char* status_code = _get_token_sequence_delimited_by(lexer, " ", &out_len, true);
	printf("Status code: %.*s\n", out_len, status_code);
	free(status_code);

	char* reason_phrase = _get_token_sequence_delimited_by(lexer, "\r\n", &out_len, true);
	printf("Reason phrase: %.*s\n", out_len, reason_phrase);
	free(reason_phrase);

	while (!_check_stream(lexer, "\r\n")) {
		char* header_key = _get_token_sequence_delimited_by(lexer, ":", &out_len, true);
		printf("%.*s: ", out_len, header_key);

		char* header_value = _get_token_sequence_delimited_by(lexer, "\r\n", &out_len, true);
		printf("%.*s\n", out_len, header_value);

		if (!strncmp(header_key, "Content-Length", 64)) {
			lexer->content_length = strtol(header_value, NULL, 10);
			printf("\tFound Content-Length header: %.*s, %d\n", out_len, header_value, lexer->content_length);
		}

		free(header_key);
		free(header_value);
	}

	if (!_match(lexer, "\r\n")) {
		printf("Failed to match CRLF at end of headers\n");
		return;
	}

	// Only parse an HTML body if Content-Length indicated one
	if (lexer->content_length == 0) {
		printf("Content-Length was zero, headers only!\n");
		_lexer_free(lexer);
		return;
	}

	uint32_t html_start_offset = lexer->read_off;
	uint32_t html_end_offset = html_start_offset + lexer->content_length;
	while (true) {
		html_tag_t* tag = _parse_html_tag(lexer);
		printf("Got tag: %s", tag->name);
		if (tag->attrs) {
			printf(" (attrs %s)\n", tag->attrs);
		}
		else {
			printf("\n");
		}

		// Optional newline?
		if (_check_newline(lexer)) {
			bool is_crlf;
			_match_newline(lexer, &is_crlf);

			// Does the HTML body end here?
			if (lexer->read_off >= html_end_offset) {
				printf("Content-Length exhausted, body parsed\n");
				_lexer_free(lexer);
				return;
			}

			// End of response? (2 CRLF in a row)
			if (is_crlf && _check_crlf(lexer)) {
				printf("Found end of HTTP response\n");
				html_tag_free(tag);
				_lexer_free(lexer);
				return;
			}
		}

		// Parse the tag content before the next tag
		// TODO(PT): One option is to build a recursive descent parser that matches the tag and everything within
		// Don't consume the opening of the next tag
		char* tag_content = _get_token_sequence_delimited_by(lexer, "<", &out_len, false);
		if (out_len) {
			printf("\t%s content: %.*s\n", tag->name, out_len, tag_content);
		}

		html_tag_free(tag);
	}

	// TODO(PT): _match should re-enqueue on failure
	// TODO(PT): Match upper and lower case
}

static void _process_amc_messages(event_loop_state_t* state) {
	do {
		amc_message_t* msg;
		amc_message_await_any(&msg);
		if (libamc_handle_message(msg)) {
			continue;
		}

		uint32_t event = amc_msg_u32_get_word(msg, 0);
		if (event == AWM_KEY_DOWN) {
			char ch = (char)amc_msg_u32_get_word(msg, 1);
			if (ch == '\n') {
				text_input_t* inp = array_lookup(state->text_inputs, 0);
				char* domain_name = inp->text;
				uint32_t domain_name_len = inp->len;
				printf("TCP: Performing DNS lookup of %.*s\n", domain_name_len, domain_name);
				uint8_t out_ipv4[IPv4_ADDR_SIZE];
				net_get_ipv4_of_domain_name(domain_name, domain_name_len, out_ipv4);
				char buf[64];
				format_ipv4_address__buf(buf, sizeof(buf), out_ipv4);
				printf("TCP: IPv4 address of %s: %s\n", domain_name, buf);

				uint32_t port = net_find_free_port();
				uint32_t dest_port = 80;
				uint32_t conn = net_tcp_conn_init(port, dest_port, out_ipv4);
				printf("TCP: Conn descriptor %d\n", conn);

				char http_buf[512];
				uint32_t len = snprintf(http_buf, sizeof(http_buf), "GET / HTTP/1.1\nHost: %s\n\n", domain_name);
				net_tcp_conn_send(conn, http_buf, len);

				_read_html_response(conn);
				/*
				uint8_t recv[1024];
				net_tcp_conn_read(conn, &recv, sizeof(recv)-1);
				text_view_t* tv = array_lookup(state->text_views, 0);
				text_box_clear(tv->text_box);
				text_box_puts(tv->text_box, recv, color_black());

				*/
				text_input_clear(inp);
			}
			else {
				text_input_t* inp = array_lookup(state->text_inputs, 0);
				if (inp->len + 1 >= inp->max_len) {
					uint32_t new_max_len = inp->max_len * 2;
					printf("Resizing text input %d -> %d\n", inp->max_len, new_max_len);
					inp->text = realloc(inp->text, new_max_len);
					inp->max_len = new_max_len;
				}
				inp->text[inp->len++] = ch;
				text_box_putchar(inp->text_box, ch, color_blue());
			}
			/*
			if (ch == 'a') {
				printf("Requesting local MAC...\n");
				uint8_t dest[MAC_ADDR_SIZE];
				net_copy_local_mac_addr(dest);
				char buf[64];
				format_mac_address(buf, sizeof(buf), dest);
				printf("Got local MAC: %s\n", buf);
			}
			else if (ch == 'r') {
				printf("Requesting local IPv4...\n");
				uint8_t dest[IPv4_ADDR_SIZE];
				net_copy_local_ipv4_addr(dest);
				char buf[64];
				format_ipv4_address__buf(buf, sizeof(buf), dest);
				printf("Got local IPv4: %s\n", buf);
			}
			else if (ch == 's') {
				printf("Requesting router IPv4...\n");
				uint8_t dest[IPv4_ADDR_SIZE];
				net_copy_router_ipv4_addr(dest);
				char buf[64];
				format_ipv4_address__buf(buf, sizeof(buf), dest);
				printf("Got router IPv4: %s\n", buf);
			}
			else if (ch == 't') {
				printf("Performing ARP resolution of something random (as a test)...\n");
				uint8_t router_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 159};
				uint8_t router_mac[MAC_ADDR_SIZE];
				net_get_mac_from_ipv4(router_ip, router_mac);
				char buf[64];
				format_mac_address(buf, sizeof(buf), router_mac);
				printf("Got router IPv4 (via ARP resolution): %s\n", buf);
			}
			else if (ch == 'd') {
				printf("Performing ARP resolution of something random (as a test)...\n");
				uint8_t router_ip[IPv4_ADDR_SIZE] = {192, 168, 1, 222};
				uint8_t router_mac[MAC_ADDR_SIZE];
				net_get_mac_from_ipv4(router_ip, router_mac);
				char buf[64];
				format_mac_address(buf, sizeof(buf), router_mac);
				printf("Got router IPv4 (via ARP resolution): %s\n", buf);
			}
			else if (ch == 'w') {
				const char* domain_name = "google.com";
				printf("Performing DNS lookup of %s\n", domain_name);
				uint8_t out_ipv4[IPv4_ADDR_SIZE];
				net_get_ipv4_of_domain_name(domain_name, strlen(domain_name), out_ipv4);
				char buf[64];
				format_ipv4_address__buf(buf, sizeof(buf), out_ipv4);
				printf("IPv4 address of %s: %s\n", domain_name, buf);
			}
			else if (ch == 'q') {
				const char* domain_name = "google.com";
				text_view_t* inp = array_lookup(state->text_inputs, 0);
				text_box_clear(inp->text_box);
				text_box_puts(inp->text_box, domain_name, color_blue());

				printf("TCP: Performing DNS lookup of %s\n", domain_name);
				uint8_t out_ipv4[IPv4_ADDR_SIZE];
				net_get_ipv4_of_domain_name(domain_name, strlen(domain_name), out_ipv4);
				char buf[64];
				format_ipv4_address__buf(buf, sizeof(buf), out_ipv4);
				printf("TCP: IPv4 address of %s: %s\n", domain_name, buf);

				uint32_t port = net_find_free_port();
				uint32_t dest_port = 80;
				uint32_t conn = net_tcp_conn_init(port, dest_port, out_ipv4);
				printf("TCP: Conn descriptor %d\n", conn);

				char http_buf[512];
				uint32_t len = snprintf(http_buf, sizeof(http_buf), "GET / HTTP/1.1\nHost: %s\n\n", domain_name);
				net_tcp_conn_send(conn, http_buf, len);

				uint8_t recv[1024];
				uint32_t read_len = net_tcp_conn_read(conn, &recv, sizeof(recv)-1);
				printf("Read %d bytes: %s\n", read_len, recv);
				recv[read_len] = '\0';
				text_view_t* tv = array_lookup(state->text_views, 0);
				text_box_clear(tv->text_box);
				text_box_puts(tv->text_box, recv, color_black());
			}
			else if (ch == 'b') {
				text_input_t* inp = array_lookup(state->text_inputs, 0);
				text_box_puts(inp->text_box, "abcdefg", color_green());
			}
			*/
		}
	} while (amc_has_message());
}

void event_loop(event_loop_state_t* state) {
	while (true) {
		// Blit views so that we draw everything once before blocking for amc

		// Blit each text input to the window layer
		for (uint32_t i = 0; i < state->text_inputs->size; i++) {
			text_input_t* ti = array_lookup(state->text_inputs, i);
			blit_layer(
				state->window_layer, 
				ti->text_box->layer, 
				ti->frame, 
				rect_make(point_zero(), ti->text_box->size)
			);
		}

		// Blit each text view to the window layer
		for (uint32_t i = 0; i < state->text_views->size; i++) {
			text_view_t* tv = array_lookup(state->text_views, i);
			blit_layer(
				state->window_layer, 
				tv->text_box->layer, 
				tv->frame, 
				rect_make(point_zero(), tv->text_box->size)
			);
		}
		amc_msg_u32_1__send(AWM_SERVICE_NAME, AWM_WINDOW_REDRAW_READY);

		_process_amc_messages(state);
	}
}
int main(int argc, char** argv) {
	amc_register_service("com.user.netclient");

	printf("Net-client running\n");
	Size window_size = size_make(800, 800);
	ca_layer* window_layer = window_layer_get(window_size.width, window_size.height);
	Rect window_frame = rect_make(point_zero(), window_size);

	event_loop_state_t* state = calloc(1, sizeof(event_loop_state_t));
	state->window_frame = window_frame;
	state->window_layer = window_layer;
	state->text_inputs = array_create(64);
	state->text_views = array_create(64);

	Size search_bar_size = size_make(window_size.width, 40);
	text_input_t* url_input = text_input_create(rect_make(point_zero(), search_bar_size), color_gray());
	array_insert(state->text_inputs, url_input);

	Size display_box_size = size_make(window_size.width, window_size.height - search_bar_size.height);
	text_view_t* display_box = text_view_create(rect_make(point_make(0, search_bar_size.height), display_box_size), color_white());
	array_insert(state->text_views, display_box);

	event_loop(state);
	// TODO(PT): Teardown window layer
	array_destroy(state->text_inputs);
	free(state);
	return 0;
}
