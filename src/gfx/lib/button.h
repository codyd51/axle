#ifndef BUTTON_H
#define BUTTON_H

#include <std/std_base.h>
#include <stdint.h>
#include "color.h"
#include "rect.h"
#include "ca_layer.h"

__BEGIN_DECLS

typedef struct button {
	//common 
	Rect frame;
	char needs_redraw;
	ca_layer* layer;
	struct view* superview;

	char* text;
	Color text_color;
} Button;

Button* create_button(Rect frame, char* text);

__END_DECLS

#endif
