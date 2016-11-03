#include "button.h"
#include "view.h"

Button* create_button(Rect frame, char* text) {
	View* button = create_view(frame);

	Label* title = create_label(frame, text);
	add_sublabel(button, title);

	button->needs_redraw = 1;
	return (Button*)button;
}
