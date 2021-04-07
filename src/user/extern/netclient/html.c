#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "elem_stack.h"
#include "html.h"

// XXX(PT): These declarations can be removed in axle
void assert(bool cond, char* msg);
uint32_t net_tcp_conn_read(uint32_t conn_desc, uint8_t* buf, uint32_t buf_size);

/*
	Utils
*/

bool str_is_whitespace(const char *s) {
	while (*s != '\0') {
		if (!isspace((unsigned char)*s)) {
			return false;
		}
		s++;
	}
	return true;
}

/*
	Stack
*/

/*
	Parser
*/

typedef struct html_dom_node html_dom_node_t;

typedef struct tcp_lexer {
	uint32_t tcp_conn_desc;
	uint8_t current_chunk[1024];
	uint32_t current_chunk_size;
	uint32_t read_off;
	uint32_t previously_read_byte_count;

	char delimiters[64];
	uint32_t delimiters_count;

	// html info
	// todo(pt): replace with a hash map of headers?
	uint32_t content_length;

	elem_stack_t* tag_stack;
	html_dom_node_t* root_node;
	html_dom_node_t* curr_deepest_tag;
} tcp_lexer_t;

static char _peekchar(tcp_lexer_t* state) {
	// have we yet to read our first chunk, or are out of data within this chunk?
	if (state->current_chunk_size == 0 || state->read_off >= state->current_chunk_size) {
		printf("lexer fetching more stream data, read_off %d curr_chunk_size %d\n", state->read_off, state->current_chunk_size);
		state->previously_read_byte_count += state->current_chunk_size;
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

	char* stream_ptr = (char*)(state->current_chunk + state->read_off);
	return !strncmp(stream_ptr, expected, expected_len);
}

static bool _check_newline(tcp_lexer_t* state) {
	return _check_stream(state, "\r\n") || _check_stream(state, "\n");
}

static bool _match_newline(tcp_lexer_t* state, bool* out_is_crlf) {
	if (out_is_crlf != NULL) {
		*out_is_crlf = false;
	}

	if (_check_stream(state, "\r\n")) {
		if (out_is_crlf != NULL) {
			*out_is_crlf = true;
		}
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

bool str_ends_with(const char *s, uint32_t s_len, const char *t, uint32_t t_len) {
	// Modified from:
	// https://codereview.stackexchange.com/questions/54722/determine-if-one-string-occurs-at-the-end-of-another/54724
	// Check if t can fit in s
    if (s_len >= t_len) {
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (s_len - t_len), t_len));
    }
	// t was longer than s
    return 0;
}

static char* _get_token_sequence_delimited_by(tcp_lexer_t* state, char* delim, uint32_t* out_len, bool consume_delim) {
	uint32_t buf_size = 2048;
	char* buf = malloc(buf_size);
	uint32_t buf_ptr = 0;
	uint32_t delim_len = strlen(delim);
	for (; buf_ptr < buf_size;) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len);
		strncpy(buf + buf_ptr, next_tok, tok_len);
		buf_ptr += tok_len;
		free(next_tok);

		// In case the delimiter was split up into multiple tokens,
		// check the last bit of the string to see if it matches the delimiter
		if (str_ends_with(buf, buf_ptr, delim, delim_len)) {
			// Trim the delimiter from the string
			buf_ptr -= strlen(delim);
			break;
		}
	}
	assert(buf_ptr < buf_size, "exceeded read buffer");

	// If we should not consume the delimiter, rewind the stream
	if (!consume_delim) {
		state->read_off -= strlen(delim);
	}

	*out_len = buf_ptr;
	char* out = strndup(buf, buf_ptr);
	free(buf);
	return out;
}

static char* _get_token_sequence_delimited_by_any(tcp_lexer_t* state, char* delims, uint32_t* out_len) {
	uint32_t buf_size = 2048;
	char* buf = malloc(buf_size);
	uint32_t buf_ptr = 0;
	uint32_t delims_count = strlen(delims);
	for (; buf_ptr < buf_size;) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len);
		strncpy(buf + buf_ptr, next_tok, tok_len);
		buf_ptr += tok_len;
		free(next_tok);

		// In case the delimiter was split up into multiple tokens,
		// check the last bit of the string to see if it matches the delimiter
		bool found_delim = false;
		for (uint32_t i = 0; i < delims_count; i++) {
			if (buf[buf_ptr-1] == delims[i]) {
				// Trim the delimiter from the string
				buf_ptr -= 1;
				found_delim = true;
				break;
			}
		}
		if (found_delim) {
			break;
		}
	}
	assert(buf_ptr < buf_size, "exceeded read buffer");

	// Rewind the stream so the delimiter is not consumed
	state->read_off -= 1;

	*out_len = buf_ptr;
	char* out = strndup(buf, buf_ptr);
	free(buf);
	return out;
}

tcp_lexer_t* _lexer_create(uint32_t conn_desc) {
	tcp_lexer_t* lexer = calloc(1, sizeof(tcp_lexer_t));
	lexer->tcp_conn_desc = conn_desc;
	char delimiters[] = {' ', '<', '>', '\r', '\n', ':', '-', '=', ',', '?', '!'};
	uint32_t delims_len = sizeof(delimiters) / sizeof(delimiters[0]);
	memcpy(lexer->delimiters, delimiters, delims_len);
	lexer->delimiters_count = delims_len;
	lexer->tag_stack = stack_create();
	return lexer;
}

void _lexer_free(tcp_lexer_t* lexer) {
	stack_destroy(lexer->tag_stack);
	free(lexer);
}

typedef struct html_tag {
	char* name;
	char* attrs;
	bool is_close_tag;
	bool requires_close_tag;
	bool is_comment;
} html_tag_t;

static html_tag_t* _parse_html_tag(tcp_lexer_t* state) {
	html_tag_t* tag = calloc(1, sizeof(html_tag_t));
	if (!_match(state, "<")) {
		printf("Failed to match open tag %c\n", _peekchar(state));
		return NULL;
	}

	// Is this a closing tag?
	if (_peekchar(state) == '/') {
		// Consume the slash and mark this is as a closing tag
		_getchar(state);
		tag->is_close_tag = true;
	}

	// Is this a comment?
	if (_check_stream(state, "!--")) {
		tag->is_comment = true;
		tag->requires_close_tag = false;
		// Consume the comment indicator
		_match(state, "!--");
		uint32_t out_len;
		tag->name = _get_token_sequence_delimited_by(state, "-->",  &out_len, false);
		// Consume the comment end
		_match(state, "-->");
		return tag;
	}

	uint32_t out_len = 0;
	//tag->name = _get_token_sequence_delimited_by(state, " ", &out_len, false);
	//tag->name = _gettok(state, &out_len);
	tag->name = _get_token_sequence_delimited_by_any(state, " >\t\n", &out_len);

	// Close tag or more attributes to follow?
	if (isspace(_peekchar(state))) {
		_getchar(state);
		tag->attrs = _get_token_sequence_delimited_by(state, ">",  &out_len, true);
	}
	else {
		// No attributes - close the tag
		if (!_match(state, ">")) {
			printf("Failed to match close tag: (name: %s), next content: %s\n", tag->name, state->current_chunk + state->read_off);
			free(tag->name);
			free(tag);
			return NULL;
		}
	}

	// Does this kind of tag require a matching closing tag?
	// TODO(PT): Come up with a way to handle casing
	tag->requires_close_tag = true;
	char* tags_without_closing_tag[] = {
		"meta",
		"hr",
		"?xml",
		"!DOCTYPE",
		"!doctype",
	};
	for (uint32_t i = 0; i < sizeof(tags_without_closing_tag) / sizeof(tags_without_closing_tag[0]); i++) {
		if (!strcmp(tag->name, tags_without_closing_tag[i])) {
			tag->requires_close_tag = false;
			break;
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

static void _print_html_tag(html_tag_t* tag) {
	printf("<%s", tag->name);
	if (tag->attrs) {
		printf(" %s>", tag->attrs);
	}
	else {
		printf(">");
	}
}

static html_dom_node_t* _html_dom_node_alloc(void) {
	html_dom_node_t* node = calloc(1, sizeof(html_dom_node_t));

	node->child_count = 0;
	node->max_children = 32;
	node->children = calloc(node->max_children, sizeof(html_dom_node_t));
	return node;
}

static html_dom_node_t* html_dom_node_create__root_node(void) {
	html_dom_node_t* node = _html_dom_node_alloc();
	node->type = HTML_DOM_NODE_TYPE_DOCUMENT;
	return node;
}

static html_dom_node_t* html_dom_node_create__from_html_tag(html_tag_t* raw_tag) {
	html_dom_node_t* node = _html_dom_node_alloc();
	node->type = HTML_DOM_NODE_TYPE_HTML_TAG;
	node->name = strdup(raw_tag->name);
	if (raw_tag->attrs) {
		node->attrs = strdup(raw_tag->attrs);
	}
	return node;
}

static html_dom_node_t* html_dom_node_create__from_text(char* text, uint32_t text_len) {
	html_dom_node_t* node = _html_dom_node_alloc();
	node->type = HTML_DOM_NODE_TYPE_TEXT;
	node->name = strndup(text, text_len);
	return node;
}

static void html_dom_node_add_child(html_dom_node_t* parent, html_dom_node_t* child) {
	if (parent->child_count + 1 >= parent->max_children) {
		uint32_t new_max = parent->max_children * 2;
		printf("Resizing node's children from %d -> %d\n", parent->max_children, new_max);
		parent->children = realloc(parent->children, sizeof(parent->children[0]) * new_max);
		parent->max_children = new_max;
	}
	parent->children[parent->child_count++] = child;
	child->parent = parent;
}


static void _process_html_tag(tcp_lexer_t* state) {
	html_tag_t* tag = _parse_html_tag(state);

	if (!tag->is_close_tag) {
		// Newly opened HTML tag

		// Add this tag to the current parent node's children
		html_dom_node_t* new_tag_node = html_dom_node_create__from_html_tag(tag);
		html_dom_node_add_child(state->curr_deepest_tag, new_tag_node);

		if (!tag->requires_close_tag) {
			printf("One-off tag: ");
			_print_html_tag(tag);
			printf("\n");
			html_tag_free(tag);
			return;
		}

		// Add a newly opened tag to our stack
		// Don't free the tag as we're holding it on the tag stack
		printf("Track newly opened tag ");
		_print_html_tag(tag);
		printf(" at read_off=%d\n", state->read_off);
		stack_push(state->tag_stack, tag);

		// The deepest tag is now the new tag
		state->curr_deepest_tag = new_tag_node;
	}
	else {
		// Closed a previously open HTML tag
		// Hopefully, the opening tag is next on the stack
		html_tag_t* open_tag = stack_pop(state->tag_stack);
		printf("Closed tag ");
		_print_html_tag(open_tag);
		printf(" with ");
		_print_html_tag(tag);
		printf("\n");
		html_tag_free(open_tag);
		html_tag_free(tag);

		// And update the deepest tag to this one's parent
		state->curr_deepest_tag = state->curr_deepest_tag->parent;
	}
}

static void _print_dom_node(html_dom_node_t* node, uint32_t depth) {
	if (depth > 0) {
		printf("\t");
		for (uint32_t i = 0; i < depth-1; i++) {
			printf("|\t");
		}
	}
	const char* node_type = "Unknown";
	switch (node->type) {
		case HTML_DOM_NODE_TYPE_DOCUMENT:
			node_type = "Root";
			break;
		case HTML_DOM_NODE_TYPE_HTML_TAG:
			node_type = "Tag";
			break;
		case HTML_DOM_NODE_TYPE_TEXT:
		default:
			node_type = "Text";
			break;
	}
	printf("<%s:%s", node_type, node->name);
	if (node->attrs) {
		printf(" (attrs: %s)", node->attrs);
	}
	printf(">\n");
	for (uint32_t i = 0; i < node->child_count; i++) {
		_print_dom_node(node->children[i], depth + 1);
	}
}

void squeezespaces(char* row, char separator) {
	// https://stackoverflow.com/questions/1458131/remove-extra-white-space-from-inside-a-c-string
	char *current = row;
	int spacing = 0;
	int i;

	for(i=0; row[i]; ++i) {
		if (isspace(row[i])) {
			if (!spacing) {
				/* start of a run of spaces -> separator */
				*current++ = separator;
				spacing = 1;
			}
		}
		else {
			*current++ = row[i];
			spacing = 0;
		}
	} 
	*current = 0;    
}

static void _whitespace_collapse(html_dom_node_t* node) {
	if (node->type == HTML_DOM_NODE_TYPE_TEXT) {
		// If the text is fully newlines/whitespace, collapse it to a single space
		if (str_is_whitespace(node->name)) {
			free(node->name);
			node->name = strdup(" ");
		}
		else {
			// The text has non-whitespace characters
			// Collapse its interior whitespace to one space
			char* copied_name = strdup(node->name);
			free(node->name);

			uint32_t len = strlen(copied_name);
			//_whitespace_reduce(copied_name);
			squeezespaces(copied_name, ' ');
			node->name = copied_name;
			/*
			for (int i = 0; i < len; i++) {
				if (isspace(copied_name[i])) {
					// Find the next non-space character
					for (int j = i; j < len; j++) {
						if (!isspace(copied_name[j])) {
							uint32_t next_non_space_idx = j;
						}
					}
				}
			}
			node->name = copied_name;
			remove_spaces(copied_name);
			*/
		}
	}

	for (uint32_t i = 0; i < node->child_count; i++) {
		_whitespace_collapse(node->children[i]);
	}
}

static void _parse_html_document(tcp_lexer_t* state, uint32_t html_end_offset) {
	printf("\nParsing HTML, end off = %d\n\n", html_end_offset);
	state->root_node = html_dom_node_create__root_node();
	state->curr_deepest_tag = state->root_node;

	while (true) {
		_process_html_tag(state);
		// TODO(PT): + 2 here, and check for CFLR+CFLR?
		if (state->read_off + state->previously_read_byte_count + 2 >= html_end_offset) {
			printf("Last char: 0x%02x\n", state->current_chunk[state->read_off+1]);
			break;
		}
		// Read anything up to the next open tag
		uint32_t out_len;
		char* content = _get_token_sequence_delimited_by(state, "<", &out_len, false);
		if (out_len) {
			//printf("read to next tag off %d out_len %d\n", state->read_off, out_len);
			html_dom_node_t* text_node = html_dom_node_create__from_text(content, out_len);
			html_dom_node_add_child(state->curr_deepest_tag, text_node);
		}
	}

	// Perform whitespace collapsing
	_whitespace_collapse(state->root_node);

	// Iterate the DOM
	_print_dom_node(state->root_node, 0);
}

html_dom_node_t* html_parse_from_socket(uint32_t conn_desc) {
	tcp_lexer_t* lexer = _lexer_create(conn_desc);

	uint32_t out_len = 0;
	printf("get_http_version\n");
	char* http_version = _get_token_sequence_delimited_by(lexer, " ", &out_len, true);
	printf("get_http_version done\n");
	printf("HTTP version: %.*s\n", out_len, http_version);
	free(http_version);

	char* status_code = _get_token_sequence_delimited_by(lexer, " ", &out_len, true);
	printf("Status code: %.*s\n", out_len, status_code);
	free(status_code);

	char* reason_phrase = _get_token_sequence_delimited_by(lexer, "\n", &out_len, true);
	printf("Reason phrase: %.*s\n", out_len, reason_phrase);
	free(reason_phrase);

	while (!_check_newline(lexer)) {
		char* header_key = _get_token_sequence_delimited_by(lexer, ": ", &out_len, true);
		printf("Header \"%.*s\": ", out_len, header_key);

		char* header_value = _get_token_sequence_delimited_by(lexer, "\n", &out_len, true);
		printf("%.*s\n", out_len, header_value);

		if (!strncmp(header_key, "Content-Length", 64)) {
			lexer->content_length = strtol(header_value, NULL, 10);
			printf("\tFound Content-Length header: %d\n", lexer->content_length);
		}

		free(header_key);
		free(header_value);
	}

	if (!_match_newline(lexer, NULL)) {
		printf("Failed to match CRLF at end of headers\n");
		return NULL;
	}

	// Only parse an HTML body if Content-Length indicated one
	if (lexer->content_length == 0) {
		printf("Content-Length was zero, headers only!\n");
		_lexer_free(lexer);
		return NULL;
	}

	uint32_t html_start_offset = lexer->read_off;
	uint32_t html_end_offset = html_start_offset + lexer->content_length;
	_parse_html_document(lexer, html_end_offset);
	// TODO(PT): Match upper and lower case
	html_dom_node_t* ret = lexer->root_node;
	_lexer_free(lexer);
	return ret;
}
