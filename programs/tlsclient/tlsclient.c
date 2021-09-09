#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <stdlibadd/assert.h>

#include <agx/lib/shapes.h>

#include <libnet/libnet.h>
#include <libgui/libgui.h>

#include "tls.h"

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

	tls_init(conn);
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

	// Testing!
	tls_init(0);

	// Enter the event loop forever
	gui_enter_event_loop(window);

	return 0;
}
