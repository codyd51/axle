#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <libamc/libamc.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>
#include <agx/lib/text_box.h>

#include <awm/awm.h>
#include <libgui/libgui.h>

#include "gfx.h"

static Rect _input_sizer(text_input_t* text_view, Size window_size) {
	return rect_make(point_zero(), window_size);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.textpad");

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("Notepad", 400, 300);
	Size window_size = window->size;

	Rect notepad_frame = rect_make(point_zero(), window_size);
	text_input_t* input = gui_text_input_create(
		window,
		notepad_frame,
		color_white(),
		(gui_window_resized_cb_t)_input_sizer
	);

	gui_enter_event_loop(window);

	return 0;
}
