#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// Layers and drawing
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>
#include <libgui/libgui.h>

// Window management
#include <awm/awm.h>

// Communication with other processes
#include <libamc/libamc.h>

#include "gfx.h"

static gui_text_view_t* _g_text_view = NULL;

static Rect _logs_text_view_sizer(gui_text_view_t* logs_text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _amc_message_received(gui_window_t* window, amc_message_t* msg) {
	char buf[msg->len+1];
	strncpy(buf, msg->body, msg->len);
	buf[msg->len] = '\0';
	//gui_text_view_puts(_g_text_view, buf, color_make(135, 20, 20));
	gui_text_view_puts(_g_text_view, buf, color_white());
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.tty");

	gui_window_t* window = gui_window_create("Logs Viewer", 700, 700);
	_g_text_view = gui_text_view_create(
		window,
		(gui_window_resized_cb_t)_logs_text_view_sizer
	);
	//logs_text_view->text_box->preserves_history = false;

	gui_add_message_handler(window, _amc_message_received);
	gui_enter_event_loop(window);

	return 0;
}
