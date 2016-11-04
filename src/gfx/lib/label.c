#include "label.h"
#include <std/std.h>

void label_teardown(Label* label) {
	if (!label) return;

	layer_teardown(label->layer);
	kfree(label->text);
	kfree(label);
}

Label* create_label(Rect frame, char* text) {
	Label* label = (Label*)kmalloc(sizeof(Label));
	label->layer = create_layer(frame.size);
	label->frame = frame;
	label->superview = NULL;
	label->text_color = color_black();
	label->needs_redraw = 1;

	label->text = strdup(text);
	return label;
}
