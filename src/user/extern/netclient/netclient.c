#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>

#include <libamc/libamc.h>
#include <stdlibadd/assert.h>

#include <awm/awm.h>

#include <libnet/libnet.h>
#include <libgui/libgui.h>

#include "html.h"
#include "layout.h"
#include "render.h"

// for draw_char
#include <agx/font/font.h>

typedef struct draw_ctx {
	Point cursor;
	Size font_size;
	Color text_color;
	gui_text_view_t* text_view;
} draw_ctx_t;

void _draw_ast(html_dom_node_t* node, uint32_t depth, gui_text_view_t* text_view) {
	Color c = color_white();
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

static void _render_html(gui_window_t* window, uint32_t tcp_conn_desc) {
	html_dom_node_t* root = html_parse_from_socket(tcp_conn_desc);
	gui_text_view_t* view = array_lookup(window->views, 0);

	if (root) {
		/*
		_render_html_dom(root, tv);

		gui_text_view_puts(tv, "\n\n\n--- HTML AST --- \n", color_dark_gray());
		_draw_ast(root, 0, tv);
		*/
		gui_layer_draw_rect(
			view->content_layer,
			rect_make(point_zero(), view->content_layer_frame.size),
			color_white(),
			THICKNESS_FILLED
		);

		// Find the style node
		html_dom_node_t* html = _html_child_tag_with_name(root, "html");
		assert(html, "no html");
		html_dom_node_t* head = _html_child_tag_with_name(html, "head");
		assert(head, "no head");
		html_dom_node_t* style = _html_child_tag_with_name(head, "style");
		assert(style, "no style");
		assert(style->child_count == 1, "wrong child count");
		html_dom_node_t* stylesheet = style->children[0];
		assert(stylesheet->type == HTML_DOM_NODE_TYPE_TEXT, "expected stylesheet text");
		array_t* css_nodes = css_parse(stylesheet->name);

		layout_root_node_t* root_layout = layout_generate(root, css_nodes, view->content_layer_frame.size.width);
		array_t* display_list = draw_commands_generate_from_layout(root_layout);
		uint32_t rect_count = 0;
		for (uint32_t i = 0; i < display_list->size; i++) {
			draw_command_t* cmd = array_lookup(display_list, i);
			if (cmd->base.cmd == DRAW_COMMAND_RECTANGLE) {

				gui_layer_draw_rect(
					view->content_layer,
					cmd->rect.rect,
					cmd->rect.color,
					cmd->rect.thickness
				);
			}
			else if (cmd->base.cmd == DRAW_COMMAND_TEXT) {
				printf("DRAW TEXT %s %d %d\n", cmd->text.text, cmd->text.font_size.width, cmd->text.font_size.height);
				Point cursor = cmd->text.rect.origin;
				for (uint32_t j = 0; j < strlen(cmd->text.text); j++) {
					char ch = cmd->text.text[j];
					gui_layer_draw_char(
						view->content_layer,
						ch,
						cursor.x,
						cursor.y,
						cmd->text.font_color,
						cmd->text.font_size
					);
					cursor.x += cmd->text.font_size.width;
				}
			}
		}
	}
	else {
		//gui_text_view_puts(tv, "\n\n\n--- Headers-Only Response ---\n", color_dark_gray());
	}
}

static void _url_bar_received_input(text_input_t* text_input, char ch) {
	if (ch == '\n') {
		char* domain_name = text_input->text;
		// Trim the newline character
		uint32_t domain_name_len = text_input->len - 1;
		printf("TCP: Performing DNS lookup of %.*s\n", domain_name_len, domain_name);
		uint8_t out_ipv4[IPv4_ADDR_SIZE];
		//net_get_ipv4_of_domain_name(domain_name, domain_name_len, out_ipv4);
		char buf[64];
		format_ipv4_address__buf(buf, sizeof(buf), out_ipv4);
		printf("TCP: IPv4 address of %s: %s\n", domain_name, buf);

		uint32_t port = net_find_free_port();
		uint32_t dest_port = 80;
		//uint32_t conn = net_tcp_conn_init(port, dest_port, out_ipv4);
		uint32_t conn = 0;
		printf("TCP: Conn descriptor %d\n", conn);

		char http_buf[512];
		uint32_t len = snprintf(http_buf, sizeof(http_buf), "GET /test HTTP/1.1\nHost: %s\n\n", domain_name);
		//net_tcp_conn_send(conn, http_buf, len);

		// Reset the URL input field
		gui_text_input_clear(text_input);

		_render_html(text_input->window, conn);
	}
}

static Rect _url_bar_sizer(text_input_t* text_input, Size window_size) {
	Size search_bar_size = size_make(window_size.width, 60);
	printf("_url_bar_sizer return %d %d\n", search_bar_size.width, search_bar_size.height);
	return rect_make(point_zero(), search_bar_size);
}

static Rect _render_box_sizer(gui_view_t* view, Size window_size) {
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

void _timer_fired(gui_window_t* w) {
	_render_html(w, 0);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.netclient");

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("Browser", 900, 800);
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
	gui_view_t* render_box = gui_view_create(
		window,
		(gui_window_resized_cb_t)_render_box_sizer
	);
	render_box->controls_content_layer = true;

	gui_timer_start(window, 0, (gui_timer_cb_t)_timer_fired, window);

	// Enter the event loop forever
	gui_enter_event_loop();

	return 0;
}
