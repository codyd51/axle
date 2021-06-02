#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <libgui/libgui.h>

static gui_text_view_t* _g_text_view = NULL;

static Rect _logs_text_view_sizer(gui_text_view_t* logs_text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _amc_message_received(amc_message_t* msg) {
	char buf[msg->len+1];
	strncpy(buf, msg->body, msg->len);
	buf[msg->len] = '\0';
	gui_text_view_puts(_g_text_view, buf, color_white());
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.logs_viewer");

	gui_window_t* window = gui_window_create("Logs Viewer", 700, 700);
	_g_text_view = gui_text_view_create(
		window,
		(gui_window_resized_cb_t)_logs_text_view_sizer
	);

	gui_add_message_handler(_amc_message_received);
	gui_enter_event_loop();

	return 0;
}
