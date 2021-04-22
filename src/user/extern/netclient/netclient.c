#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

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

#include "html.h"

typedef struct draw_ctx {
	Point cursor;
	Size font_size;
	Color text_color;
	text_view_t* text_view;
} draw_ctx_t;

void _h1_enter(draw_ctx_t* ctx) {
	ctx->font_size = size_make(24, 36);
	ctx->text_view->text_box->font_size = ctx->font_size;
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
}

void _h1_exit(draw_ctx_t* ctx) {
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->font_size = size_make(8, 12);
	ctx->text_view->text_box->font_size = ctx->font_size;
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
}

void _h2_enter(draw_ctx_t* ctx) {
	ctx->font_size = size_make(16, 24);
	ctx->text_view->text_box->font_size = ctx->font_size;
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
}

void _h2_exit(draw_ctx_t* ctx) {
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->font_size = size_make(8, 12);
	ctx->text_view->text_box->font_size = ctx->font_size;
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
}

void _p_enter(draw_ctx_t* ctx) {
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
}

void _p_exit(draw_ctx_t* ctx) {
	gui_text_view_puts(ctx->text_view, "\n", ctx->text_color);
	ctx->cursor = ctx->text_view->text_box->cursor_pos;
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

void _draw_ast(html_dom_node_t* node, uint32_t depth, text_view_t* text_view) {
	Color c = color_black();
	if (depth > 0) {
		gui_text_view_puts(text_view, "\t", c);
		for (uint32_t i = 0; i < depth-1; i++) {
			gui_text_view_puts(text_view, "\t", c);
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
	gui_text_view_puts(text_view, buf, c);
	if (node->attrs) {
		snprintf(buf, sizeof(buf), " (attrs: %s)", node->attrs);
		gui_text_view_puts(text_view, buf, c);
	}
	gui_text_view_puts(text_view, ">\n", c);
	for (uint32_t i = 0; i < node->child_count; i++) {
		_draw_ast(node->children[i], depth + 1, text_view);
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

void _draw_node(html_dom_node_t* node, uint32_t depth, draw_ctx_t* ctx, text_view_t* text_view) {
	// TODO(PT): Draw into a text box or ca_layer? Probably the latter
	html_tag_callback_t* active_tag = NULL;
	ctx->text_view = text_view;

	for (uint32_t i = 0; i < depth; i++) {
		printf("\t");
	}

	if (node->type == HTML_DOM_NODE_TYPE_TEXT) {
		printf("Drawing text at (%d, %d): %s\n", ctx->cursor.x, ctx->cursor.y, node->name);
		// Make sure the text box draws at the right place
		text_view->text_box->cursor_pos = ctx->cursor;
		text_view->text_box->font_size = ctx->font_size;
		gui_text_view_puts(text_view, node->name, ctx->text_color);
		// And update our cursor to reflect what the text box drew
		ctx->cursor = text_view->text_box->cursor_pos;
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
		_draw_node(node->children[i], depth + 1, ctx, text_view);
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

static void _render_html_dom(html_dom_node_t* root_node, text_view_t* text_view) {
	draw_ctx_t ctx = {0};
	// Default parameters
	gui_text_view_puts(text_view, "--- Rendered HTML --- \n", color_dark_gray());
	ctx.cursor = text_view->text_box->cursor_pos;
	ctx.font_size = text_view->text_box->font_size;
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
	gui_text_view_puts(text_view, "Page title: ", color_dark_gray());
	gui_text_view_puts(text_view, title_str, color_light_gray());
	gui_text_view_puts(text_view, "\n", color_black());

	// And the boxy
	_draw_node(body_node, 0, &ctx, text_view);
}

static void _url_bar_received_input(text_input_t* text_input, char ch) {
	if (ch == '\n') {
		char* domain_name = text_input->text;
		// Trim the newline character
		uint32_t domain_name_len = text_input->len - 1;
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

		// Reset the URL input field
		gui_text_input_clear(text_input);

		printf("Calling html_parse_from_socket(%d)\n", conn);

		html_dom_node_t* root = html_parse_from_socket(conn);
		text_view_t* tv = array_lookup(text_input->window->text_views, 0);

		if (root) {
			_render_html_dom(root, tv);

			gui_text_view_puts(tv, "\n\n\n--- HTML AST --- \n", color_dark_gray());
			_draw_ast(root, 0, tv);
		}
		else {
			gui_text_view_puts(tv, "\n\n\n--- Headers-Only Response ---\n", color_dark_gray());
		}
	}
}

static Rect _url_bar_sizer(text_input_t* text_input, Size window_size) {
	Size search_bar_size = size_make(window_size.width, 60);
	printf("_url_bar_sizer return %d %d\n", search_bar_size.width, search_bar_size.height);
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
	amc_register_service("com.user.netclient");
	printf("Net-client running\n");

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("Browser", 800, 800);
	Size window_size = window->size;

	// Set up the search bar and render box GUI elements
	Size search_bar_size = size_make(window_size.width, 60);
	text_input_t* url_input = gui_text_input_create(
		window,
		rect_make(point_zero(), search_bar_size), 
		color_white(),
		(gui_window_resized_cb_t)_url_bar_sizer
	);
	url_input->text_box->font_size = size_make(12, 20);
	url_input->text_entry_cb = (text_input_text_entry_cb_t)_url_bar_received_input;
	gui_text_input_set_prompt(url_input, "Enter a URL: ");

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
