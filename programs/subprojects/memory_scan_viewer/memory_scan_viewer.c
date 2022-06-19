#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <libgui/libgui.h>

static Rect _text_view_sizer(gui_text_view_t* text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

gui_text_view_t* _g_text_view = NULL;

void _message_handler(amc_message_t* message) {
	if (strncmp(message->source, "com.dangerous.memory_walker", AMC_MAX_SERVICE_NAME_LEN)) {
		return;
	}

	char* string = (char*)message->body;
	gui_text_view_puts(_g_text_view, string, color_white());
	gui_text_view_putchar(_g_text_view, '\n', color_white());

	// TODO(PT): printf in the kernel is interpreting extra %i in %s as formats! 

	/*
	char buf[message->len+2];
	strncpy(buf, message->body, message->len);
	buf[message->len] = '\n';
	buf[message->len+1] = '\0';
	printf("Displaying: %s", buf);
	gui_text_view_puts(_g_text_view, buf, color_white());
	*/

	_g_text_view->content_layer->scroll_layer.scroll_offset.y = (_g_text_view->content_layer->scroll_layer.max_y - _g_text_view->content_layer_frame.size.height + _g_text_view->font_size.height);
}

int main(int argc, char** argv) {
	amc_register_service("com.dangerous.memory_scan_viewer");

	gui_window_t* window = gui_window_create("Memory Scan Results", 1100, 600);
	_g_text_view = gui_text_view_alloc();
	gui_text_view_init(_g_text_view, window, (gui_window_resized_cb_t)_text_view_sizer);
	_g_text_view->font_size = size_make(10, 14);
	gui_text_view_add_to_window(_g_text_view, window);

	gui_add_message_handler(_message_handler);
	gui_enter_event_loop();

	return 0;
}
