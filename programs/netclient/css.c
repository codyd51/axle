#include <string.h>
#include <memory.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "css.h"
#include "utils.h"

typedef struct css_lexer {
	uint8_t* buf;
	uint32_t buf_len;
	uint32_t read_off;

	char delimiters[32];
	uint32_t delimiters_count;
} css_lexer_t;

static char _peekchar(css_lexer_t* state, bool* eof) {
	// have we yet to read our first chunk, or are out of data within this chunk?
	if (state->buf_len == 0 || state->read_off >= state->buf_len) {
		*eof = true;
		return 0;
	}

	return state->buf[state->read_off];
}

static char _getchar(css_lexer_t* state, bool* eof) {
	char ret = _peekchar(state, eof);
	state->read_off += 1;
	return ret;
}

static bool _char_is_delimiter(css_lexer_t* state, char ch) {
	for (uint32_t i = 0; i < state->delimiters_count; i++) {
		if (ch == state->delimiters[i]) {
			return true;
		}
	}
	return false;
}

static char* _gettok(css_lexer_t* state, uint32_t* out_len, bool* eof) {
	char buf[128];
	uint32_t i = 0;
	for (; i < sizeof(buf); i++) {
		char peek = _peekchar(state, eof);
		if (*eof) return NULL;

		// Skip over whitespace
		if (peek == ' ' || peek == '\r' || peek == '\n') {
			// Decrement i to keep our position in the buffer correct
			i -= 1;
			_getchar(state, eof);
			if (*eof) return NULL;
			continue;
		}

		bool is_delimiter = _char_is_delimiter(state, peek);
		if (is_delimiter) {
			if (i > 0) {
				// if this is a multi-character token, break if we encounter a delimiter
				break;
			}
			else {
				// 1-character delimiter token
				// ensure `i` is incremented to reflect a token, even though we break
				buf[i++] = _getchar(state, eof);
				if (*eof) return NULL;
				break;
			}
		}

		// normal token
		buf[i] = _getchar(state, eof);
		if (*eof) return NULL;
	}
	*out_len = i;
	return strndup(buf, i);
}

static char* _peektok(css_lexer_t* state, uint32_t* out_len, bool* eof) {
	char buf[128];
	uint32_t i = 0;
	uint32_t start_pos = state->read_off;
	for (; i < sizeof(buf); i++) {
		char peek = _peekchar(state, eof);
		if (*eof) return NULL;

		// Skip over whitespace
		if (peek == ' ' || peek == '\r' || peek == '\n') {
			// Decrement i to keep our position in the buffer correct
			i -= 1;
			_getchar(state, eof);
			if (*eof) return NULL;
			continue;
		}

		bool is_delimiter = _char_is_delimiter(state, peek);
		if (is_delimiter) {
			if (i > 0) {
				// if this is a multi-character token, break if we encounter a delimiter
				break;
			}
			else {
				// 1-character delimiter token
				// ensure `i` is incremented to reflect a token, even though we break
				buf[i++] = _getchar(state, eof);
				if (*eof) return NULL;
				break;
			}
		}

		// normal token
		buf[i] = _getchar(state, eof);
		if (*eof) return NULL;
	}
	*out_len = i;
	state->read_off = start_pos;
	return strndup(buf, i);
}

static bool _match(css_lexer_t* state, char* expected, bool* eof) {
	char buf[256];
	uint32_t buf_ptr = 0;
	uint32_t expected_len = strlen(expected);
	for (; buf_ptr < sizeof(buf);) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len, eof);
		if (*eof) return NULL;

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

static char* _get_token_sequence_delimited_by(css_lexer_t* state, char* delim, uint32_t* out_len, bool consume_delim, bool* eof) {
	uint32_t buf_size = 2048;
	char* buf = malloc(buf_size);
	uint32_t buf_ptr = 0;
	uint32_t delim_len = strlen(delim);
	for (; buf_ptr < buf_size;) {
		uint32_t tok_len = 0;
		char* next_tok = _gettok(state, &tok_len, eof);
		if (*eof) return NULL;

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

css_node_t* _css_parse_rule(css_lexer_t* state, bool* eof) {
	css_node_t* node = calloc(1, sizeof(css_node_t));

	uint32_t tok_len = 0;
	char* selector = _gettok(state, &tok_len, eof);
	if (*eof) return NULL;

	node->name = strdup(selector);
	printf("CSS selector: %s\n", selector);

	assert(_match(state, "{", eof), "Expected brace after selector");

	while (true) {
		char* property = _get_token_sequence_delimited_by(state, ":", &tok_len, true, eof);
		printf("\tProperty: %s\n", property);

		char* value = _get_token_sequence_delimited_by(state, ";", &tok_len, true, eof);
		printf("\tValue: %s\n", value);

		if (!strncmp(property, "background-color", 32)) {
			uint32_t r, g, b;
			sscanf(value, "#%02x%02x%02x", &r, &g, &b);
			node->sets_background_color = true;
			node->background_color = color_make(r, g, b);
		}
		else if (!strncmp(property, "margin-top", 32)) {
			uint32_t margin_top;
			sscanf(value, "%dpx", &margin_top);
			node->sets_margin_top = true;
			node->margin_top = margin_top;
		}
		else if (!strncmp(property, "margin-bottom", 32)) {
			uint32_t margin_bottom;
			sscanf(value, "%dpx", &margin_bottom);
			node->sets_margin_bottom = true;
			node->margin_bottom = margin_bottom;
		}
		else if (!strncmp(property, "margin-left", 32)) {
			uint32_t margin_left;
			sscanf(value, "%dpx", &margin_left);
			node->sets_margin_left = true;
			node->margin_left = margin_left;
		}
		else if (!strncmp(property, "margin-right", 32)) {
			uint32_t margin_right;
			sscanf(value, "%dpx", &margin_right);
			node->sets_margin_right = true;
			node->margin_right = margin_right;
		}
		else if (!strncmp(property, "font-size", 32)) {
			uint32_t font_size_em;
			// TODO(PT): Error handling if format is different
			sscanf(value, "%dem", &font_size_em);
			node->sets_font_size_em = true;
			node->font_size_em = font_size_em;
		}
		else if (!strncmp(property, "color", 32)) {
			uint32_t r, g, b;
			sscanf(value, "#%02x%02x%02x", &r, &g, &b);
			node->sets_font_color = true;
			node->font_color = color_make(r, g, b);
		}
		else {
			printf("Unknown CSS property name: %s { %s: %s; }\n", selector, property, value);
		}

		free(property);
		free(value);

		// Do we have more declarations within this rule to parse?
		char* peek = _peektok(state, &tok_len, eof);
		if (*eof) return NULL;
		bool finished_rule = !strncmp(peek, "}", 1);
		free(peek);
		if (finished_rule) {
			break;
		}
	}

	free(selector);

	assert(_match(state, "}", eof), "Expected closing brace");
	return node;
}

static void _lexer_teardown(css_lexer_t* lexer) {
	free(lexer->buf);
	free(lexer);
}

array_t* css_parse(const char* stylesheet) {
	array_t* nodes = array_create(32);

	css_lexer_t* lexer = calloc(1, sizeof(css_lexer_t));
	lexer->buf = (uint8_t*)strdup(stylesheet);
	lexer->buf_len = strlen((char*)lexer->buf);
	char delimiters[] = {' ', '<', '>', '\r', '\n', ':', '-', '=', ',', '?', '!', '{', '}', '#', ';'};
	uint32_t delims_len = sizeof(delimiters) / sizeof(delimiters[0]);
	memcpy(lexer->delimiters, delimiters, delims_len);
	lexer->delimiters_count = delims_len;

	while (lexer->read_off <= lexer->buf_len) {
		bool eof = false;
		css_node_t* node = _css_parse_rule(lexer, &eof);
		if (eof) {
			printf("Hit EOF while parsing CSS node!\n");
			break;
		}

		printf("<CSS Node: %s>\n", node->name);

		if (node->sets_background_color) {
			printf("\tBackground color: (%d, %d, %d)\n", node->background_color.val[0], node->background_color.val[1], node->background_color.val[2]);
		}
		if (node->sets_font_size_em) {
			printf("\tFont em multiple: %d\n", node->font_size_em);
		}
		if (node->sets_margin_top) {
			printf("\tMargin top: %d\n", node->margin_top);
		}

		array_insert(nodes, node);

		// Do we have more rules to parse?
		uint32_t tok_len;
		char* peek = _peektok(lexer, &tok_len, &eof);
		if (eof) {
			assert(!peek, "EOF hit but non-NULL token returned");
			break;
		}
		free(peek);
	}

	_lexer_teardown(lexer);

	return nodes;
}

void css_tree_print(css_node_t* root) {
	printf("css_tree_print\n");
}
