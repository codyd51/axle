#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <libgui/libgui.h>

// Communication with other processes
#include <libamc/libamc.h>

#include <libport/libport.h>
#include <libutils/assert.h>

#include "terminal.h"

static Rect _text_input_sizer(gui_text_input_t* tv, Size window_size) {
	return rect_make(point_zero(), window_size);
}

static void _shell_print_prompt(gui_text_input_t* view) {
	gui_text_view_puts((gui_text_view_t*)view, "axle> ", view->text_color);
}

static void _key_down(gui_text_input_t* ti, uint32_t ch) {
    if (ch == '\n') {
        printf("Launching program\n");
        _shell_print_prompt(ti);
    }
    else {
        printf("Appending character...\n");
    }
}

int main(int argc, char** argv) {
	amc_register_service(TERMINAL_SERVICE_NAME);

    gui_window_t* window = gui_window_create("Terminal", 600, 400);
    gui_text_input_t* text_input = gui_text_input_create(
        window,
        (gui_window_resized_cb_t)_text_input_sizer
    );
    text_input->key_down_cb = (gui_key_down_cb_t)_key_down;
    text_input->text_color = color_green();

    // Iterate the PCI devices and draw them into the text box
    //Color text_color = color_green();

    _shell_print_prompt(text_input);

    gui_enter_event_loop();

	return 0;
}
