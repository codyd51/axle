#include "label.h"
#include "view.h"
#include <std/std.h>
#include <gfx/lib/shapes.h>

void label_teardown(Label* label) {
	if (!label) return;

	layer_teardown(label->layer);
	kfree(label->text);
	kfree(label);
}

void set_text(Label* label, char* text) {
	kfree(label->text);
	label->text = strdup(text);
	mark_needs_redraw((View*)label);
}

Label* create_label(Rect frame, char* text) {
	Label* label = (Label*)kmalloc(sizeof(Label));
	label->layer = create_layer(frame.size);
	label->frame = frame;
	label->superview = NULL;
	label->text_color = color_black();
	label->needs_redraw = 1;
	label->font_size = size_make(CHAR_WIDTH, CHAR_HEIGHT);

	label->text = strdup(text);
	return label;
}

void draw_label(ca_layer* dest, Label* label) {
	if (!label) return;

	View* superview = label->superview;

	Color background_color = color_white();
	//try to match text bounding box to superview's background color
	if (superview) {
		background_color = superview->background_color;
	}
	draw_rect(label->layer, rect_make(point_zero(), label->frame.size), background_color, THICKNESS_FILLED);

	Point origin = point_zero();
	/*
	if (label->frame.size.width >= CHAR_WIDTH && label->frame.size.height >= CHAR_HEIGHT) {
		origin.x = CHAR_WIDTH;
		origin.y = CHAR_HEIGHT;
	}
	*/
	draw_string(label->layer, label->text, origin, label->text_color, label->font_size);

	blit_layer(dest, label->layer, rect_make(label->frame.origin, label->layer->size), rect_make(point_zero(), label->layer->size));

	label->needs_redraw = 0;
}

