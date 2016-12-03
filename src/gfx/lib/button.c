#include "button.h"
#include "view.h"
#include <gfx/font/font.h>
#include <std/std.h>

void button_handle_mousedown(Button* button) {
	button->toggled = true;

	if (button->mousedown_handler) {
		event_fp handler = button->mousedown_handler;
		handler(button);
	}
}

void button_handle_mouseup(Button* button) {
	button->toggled = false;

	if (button->mouseup_handler) {
		event_fp handler = button->mouseup_handler;
		handler(button);
	}
}

Button* create_button(Rect frame, char* text) {
	Button* button = kmalloc(sizeof(Button));
	button->frame = frame;
	button->superview = NULL;

	int label_width = strlen(text) * CHAR_WIDTH;
	Label* title = create_label(rect_make(point_make(frame.origin.x + (frame.size.width * 0.5) - (label_width / 2), frame.origin.y - CHAR_HEIGHT + frame.size.height * 0.5), size_make(label_width, CHAR_HEIGHT)), text);
	button->label = title;

	button->mousedown_handler = NULL;
	button->mouseup_handler = NULL;

	button->needs_redraw = 1;
	return button;
}
