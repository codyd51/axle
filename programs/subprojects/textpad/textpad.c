#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <libgui/libgui.h>

static Rect _input_sizer(gui_text_input_t* text_input, Size window_size) {
	return rect_make(point_zero(), window_size);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.textpad");

	// Instantiate the GUI window
	gui_window_t* window = gui_window_create("Notepad", 400, 300);
	Size window_size = window->size;

	Rect notepad_frame = rect_make(point_zero(), window_size);
	gui_text_input_t* input = gui_text_input_create(
		window,
		(gui_window_resized_cb_t)_input_sizer
	);
	input->background_color = color_white();
	// Necessary to display the updated background color
	gui_text_view_clear(input);

	gui_enter_event_loop();

	return 0;
}
