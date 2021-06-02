#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <libgui/libgui.h>

#include "crash_reporter_messages.h"

static gui_text_view_t* _g_text_view = NULL;

static Rect _text_view_sizer(gui_text_view_t* logs_text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _amc_message_received(amc_message_t* msg) {
    const char* source_service = msg->source;

	crash_reporter_inform_assert_t* assert_event = (crash_reporter_inform_assert_t*)&msg->body;
	assert(assert_event->event == CRASH_REPORTER_INFORM_ASSERT, "Expected inform assert message");

	printf("Received assertion from %s\n", msg->source);
	char buf[512];
	snprintf(buf, sizeof(buf), "Received assertion from %s:\n%s\n", msg->source, assert_event->assert_message);
	gui_text_view_puts(_g_text_view, buf, color_green());
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.crash_reporter");

	gui_window_t* window = gui_window_create("Crash Reporter", 500, 300);
	_g_text_view = gui_text_view_create(
		window,
		(gui_window_resized_cb_t)_text_view_sizer
	);
	gui_add_message_handler(_amc_message_received);
	gui_enter_event_loop();

	return 0;
}
