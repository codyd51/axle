#include "button.h"
#include "view.h"
#include <gfx/font/font.h>
#include <std/std.h>
#include <gfx/lib/shapes.h>

void button_handle_mousedown(Button* button) {
	button->toggled = true;

	if (button->mousedown_handler) {
		event_handler handler = button->mousedown_handler;
		handler(button, NULL);
	}
}

void button_handle_mouseup(Button* button) {
	button->toggled = false;

	if (button->mouseup_handler) {
		event_handler handler = button->mouseup_handler;
		handler(button, NULL);
	}
}

Button* create_button(Rect frame, char* text) {
	Button* button = kmalloc(sizeof(Button));
	button->frame = frame;
	button->superview = NULL;

	int label_width = strlen(text) * (CHAR_WIDTH + font_padding_for_size(size_make(CHAR_WIDTH, CHAR_HEIGHT)).width);
	Label* title = create_label(rect_make(point_make(frame.origin.x + (frame.size.width * 0.5) - (label_width / 2), frame.origin.y - CHAR_HEIGHT + frame.size.height * 0.5), size_make(label_width, CHAR_HEIGHT)), text);
	button->label = title;

	button->mousedown_handler = NULL;
	button->mouseup_handler = NULL;

	button->needs_redraw = 1;
	return button;
}

void draw_button(ca_layer* dest, Button* button) {
	if (!button || !dest) return;

	Color background_color = color_gray();
	if (button->toggled) {
		background_color = color_dark_gray();
	}

	//background
	draw_rect(dest, button->frame, background_color, THICKNESS_FILLED);
	//button border
	draw_rect(dest, button->frame, color_black(), 1);
	//title	
	draw_label(dest, button->label);

	button->needs_redraw = 0;
}

