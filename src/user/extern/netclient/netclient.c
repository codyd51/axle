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

#include "html.h"

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
	text_view->text_box->preserves_history = true;
	text_view->text_box->cache_drawing = true;
	text_view->frame = frame;
	return text_view;
}

typedef struct draw_ctx {
	Point cursor;
	Size font_size;
	Color text_color;
	text_box_t* text_box;
} draw_ctx_t;

void _h1_enter(draw_ctx_t* ctx) {
	ctx->font_size = size_make(24, 36);
	ctx->text_box->font_size = ctx->font_size;
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _h1_exit(draw_ctx_t* ctx) {
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->font_size = size_make(8, 12);
	ctx->text_box->font_size = ctx->font_size;
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _h2_enter(draw_ctx_t* ctx) {
	ctx->font_size = size_make(16, 24);
	ctx->text_box->font_size = ctx->font_size;
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _h2_exit(draw_ctx_t* ctx) {
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->font_size = size_make(8, 12);
	ctx->text_box->font_size = ctx->font_size;
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _p_enter(draw_ctx_t* ctx) {
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _p_exit(draw_ctx_t* ctx) {
	text_box_puts(ctx->text_box, "\n", ctx->text_color);
	ctx->cursor = ctx->text_box->cursor_pos;
}

void _a_enter(draw_ctx_t* ctx) {
	ctx->text_color = color_blue();
}

void _a_exit(draw_ctx_t* ctx) {
	// TODO(PT): Model a stack of attributes
	// When entering a new, push the attr
	// When exiting, pop
	// Get the "effective style" by applying the stack from bottom to top
	ctx->text_color = color_black();
}

void _draw_ast(html_dom_node_t* node, uint32_t depth, text_box_t* text_box) {
	Color c = color_black();
	if (depth > 0) {
		text_box_puts(text_box, "\t", c);
		for (uint32_t i = 0; i < depth-1; i++) {
			text_box_puts(text_box, "|\t", c);
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
	char buf[256];
	snprintf(buf, sizeof(buf), "<%s:%s", node_type, node->name);
	text_box_puts(text_box, buf, c);
	if (node->attrs) {
		snprintf(buf, sizeof(buf), " (attrs: %s)", node->attrs);
		text_box_puts(text_box, buf, c);
	}
	text_box_puts(text_box, ">\n", c);
	for (uint32_t i = 0; i < node->child_count; i++) {
		_draw_ast(node->children[i], depth + 1, text_box);
	}
}

typedef struct html_tag_callback {
	const char* tag_name;
	void(*enter)(draw_ctx_t*);
	void(*exit)(draw_ctx_t*);
} html_tag_callback_t;

html_tag_callback_t html_tags[] = {
	{"h1", _h1_enter, _h1_exit},
	{"H1", _h1_enter, _h1_exit},
	{"h2", _h2_enter, _h2_exit},
	{"H2", _h2_enter, _h2_exit},
	{"p", _p_enter, _h2_exit},
	{"P", _p_enter, _p_exit},
	{"a", _a_enter, _a_exit},
	{"A", _a_enter, _a_exit},
};

void _draw_node(html_dom_node_t* node, uint32_t depth, draw_ctx_t* ctx, text_box_t* text_box) {
	// TODO(PT): Draw into a text box or ca_layer? Probably the latter
	html_tag_callback_t* active_tag = NULL;
	ctx->text_box = text_box;

	for (uint32_t i = 0; i < depth; i++) {
		printf("\t");
	}

	if (node->type == HTML_DOM_NODE_TYPE_TEXT) {
		printf("Drawing text at (%d, %d): %s\n", ctx->cursor.x, ctx->cursor.y, node->name);
		// Make sure the text box draws at the right place
		text_box->cursor_pos = ctx->cursor;
		text_box->font_size = ctx->font_size;
		text_box_puts(text_box, node->name, ctx->text_color);
		// And update our cursor to reflect what the text box drew
		ctx->cursor = text_box->cursor_pos;
	}
	else if (node->type == HTML_DOM_NODE_TYPE_HTML_TAG) {
		if (!strcmp(node->name, "style")) {
			// Skip style tags and their contents
			return;
		}

		for (uint32_t i = 0; i < sizeof(html_tags) / sizeof(html_tags[0]); i++) {
			html_tag_callback_t* ent = &html_tags[i];
			if (!strcmp(node->name, ent->tag_name)) {
				// Add the attributes of this tag to the attributes stack
				printf("Adding attributes of tag %s\n", node->name);
				active_tag = ent;
				ent->enter(ctx);
				break;
			}
		}
	}
	else {
		printf("Doing no drawing for DOM node of type %d: %s\n", node->type, node->name);
	}

	for (uint32_t i = 0; i < node->child_count; i++) {
		_draw_node(node->children[i], depth + 1, ctx, text_box);
	}

	if (node->type == HTML_DOM_NODE_TYPE_HTML_TAG) {
		if (active_tag) {
			active_tag->exit(ctx);
		}
		else {
			printf("Warning: No active_tag set up for HTML tag name: %s\n", node->name);
		}
	}
}

static html_dom_node_t* _html_child_tag_with_name(html_dom_node_t* parent_node, char* tag_name) {
	for (uint32_t i = 0; i < parent_node->child_count; i++) {
		html_dom_node_t* child_node = parent_node->children[i];
		if (child_node->type == HTML_DOM_NODE_TYPE_HTML_TAG) {
			// https://stackoverflow.com/questions/2661766/how-do-i-lowercase-a-string-in-c
			char* lower = strdup(child_node->name);
			char* p = lower;
			for ( ; *p; ++p) *p = tolower(*p);

			if (!strcmp(lower, tag_name)) {
				free(lower);
				return child_node;
			}
			free(lower);
		}
	}
	return NULL;
}

static char* _html_child_text(html_dom_node_t* parent_node) {
	for (uint32_t i = 0; i < parent_node->child_count; i++) {
		html_dom_node_t* child_node = parent_node->children[i];
		if (child_node->type == HTML_DOM_NODE_TYPE_TEXT) {
			return child_node->name;
		}
	}
	return NULL;
}

static void _render_html_dom(html_dom_node_t* root_node, text_box_t* text_box) {
	draw_ctx_t ctx = {0};
	// Default parameters
	text_box_puts(text_box, "--- Rendered HTML --- \n", color_dark_gray());
	ctx.cursor = text_box->cursor_pos;
	ctx.font_size = text_box->font_size;
	ctx.text_color = color_black();

	// Find the page title
	html_dom_node_t* html_node = _html_child_tag_with_name(root_node, "html");
	if (!html_node) {
		printf("No <html> tag!\n");
		return;
	}
	html_dom_node_t* head_node = _html_child_tag_with_name(html_node, "head");
	if (!head_node) {
		printf("No <head> tag within <html>!\n");
		return;
	}
	html_dom_node_t* title_node = _html_child_tag_with_name(head_node, "title");
	char* title_str = NULL;
	if (title_node) {
		title_str = _html_child_text(title_node);
	}
	title_str = title_str ?: "No page title";

	// And find the body
	html_dom_node_t* body_node = _html_child_tag_with_name(html_node, "body");
	if (!body_node) {
		printf("No <body> tag within <html>!\n");
		return;
	}

	// Draw the page title
	text_box_puts(text_box, "Page title: ", color_dark_gray());
	text_box_puts(text_box, title_str, color_light_gray());
	text_box_puts(text_box, "\n", color_black());

	// And the boxy
	_draw_node(body_node, 0, &ctx, text_box);
}

static void _process_amc_messages(event_loop_state_t* state) {
	bool got_resize_msg = false;
	awm_window_resized_msg_t newest_resize_msg = {0};

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

				printf("Calling html_parse_from_socket(%d)\n", conn);
				html_dom_node_t* root = html_parse_from_socket(conn);

				text_view_t* tv = array_lookup(state->text_views, 0);
				_render_html_dom(root, tv->text_box);

				text_box_puts(tv->text_box, "\n\n\n--- HTML AST-- \n", color_dark_gray());
				_draw_ast(root, 0, tv->text_box);
				/*
				uint8_t recv[1024];
				net_tcp_conn_read(conn, &recv, sizeof(recv)-1);
				text_box_clear(tv->text_box);
				text_box_puts(tv->text_box, recv, color_black());

				*/
				text_input_clear(inp);
				text_box_puts(inp->text_box, "Enter a URL: ", color_make(200, 200, 200));
			}
			else if (ch == '\b') {
				// TODO(PT): text_box_delchar?
				text_input_t* inp = array_lookup(state->text_inputs, 0);
				if (inp->len > 0) {
					inp->text[--inp->len] = '\0';
					text_box_t* text_box = inp->text_box;

					text_box->cursor_pos.x -= text_box->font_size.width + text_box->font_padding.width;
					text_box_putchar(text_box, ' ', text_box->background_color);
					text_box->cursor_pos.x -= text_box->font_size.width + text_box->font_padding.width;
				}
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
				text_box_putchar(inp->text_box, ch, color_white());
			}
		}
		else if (event == AWM_MOUSE_SCROLLED) {
			text_view_t* tv = array_lookup(state->text_views, 0);
			awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
			bool scroll_up = m->delta_z > 0;
			for (uint32_t i = 0; i < abs(m->delta_z); i++) {
				if (scroll_up) text_box_scroll_up(tv->text_box);
				else text_box_scroll_down(tv->text_box);
			}
		}
		else if (event == AWM_WINDOW_RESIZED) {
			got_resize_msg = true;
			awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)msg->body;
			newest_resize_msg = *m;
		}
	} while (amc_has_message());

	if (got_resize_msg) {
		awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)&newest_resize_msg;
		state->window_frame.size = m->new_size;

		Size search_bar_size = size_make(state->window_frame.size.width, 40);
		//text_input_t* url_input = text_input_create(rect_make(point_zero(), search_bar_size), color_dark_gray());

		Size display_box_size = size_make(state->window_frame.size.width, state->window_frame.size.height - search_bar_size.height);
		text_view_t* tv = array_lookup(state->text_views, 0);
		tv->frame = rect_make(
			point_make(0, search_bar_size.height),
			display_box_size
		);
		text_box_resize(tv->text_box, tv->frame.size);
	}
}

void event_loop(event_loop_state_t* state) {
	while (true) {
		// Blit views so that we draw everything once before blocking for amc

		// Blit each text input to the window layer
		for (uint32_t i = 0; i < state->text_inputs->size; i++) {
			text_input_t* ti = array_lookup(state->text_inputs, i);
			text_box_blit(ti->text_box, state->window_layer, ti->frame);
			// Draw the input indicator
			draw_rect(
				state->window_layer, 
				rect_make(
					ti->text_box->cursor_pos,
					size_make(5, ti->text_box->font_size.height)
				),
				color_white(),
				THICKNESS_FILLED
			);
		}

		// Blit each text view to the window layer
		for (uint32_t i = 0; i < state->text_views->size; i++) {
			text_view_t* tv = array_lookup(state->text_views, i);
			text_box_blit(tv->text_box, state->window_layer, tv->frame);
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
	text_input_t* url_input = text_input_create(rect_make(point_zero(), search_bar_size), color_dark_gray());
	url_input->text_box->font_size = size_make(16, 24);
	text_box_puts(url_input->text_box, "Enter a URL: ", color_make(200, 200, 200));
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
