#include "label.h"
#include "view.h"
#include <std/std.h>

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
