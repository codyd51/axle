#include "button.h"
#include "view.h"
#include <gfx/font/font.h>
#include <std/std.h>

void button_handle_click(Button* button) {
	button->toggled = !button->toggled;

	if (button->mousedown_handler) {
		mousedown_fp handler = button->mousedown_handler;
		handler(button);
	}
}

Button* create_button(Rect frame, char* text) {
	Button* button = kmalloc(sizeof(Button));
	button->frame = frame;
	button->superview = NULL;

	Label* title = create_label(rect_make(point_make(frame.origin.x + frame.size.width * 0.125, frame.origin.y + frame.size.height * 0.25), size_make(frame.size.width * 0.75, CHAR_HEIGHT)), text);
	button->label = title;

	button->mousedown_handler = NULL;

	button->needs_redraw = 1;
	return button;
}
