#ifndef BUTTON_H
#define BUTTON_H

#include <std/std_base.h>
#include <stdint.h>
#include "color.h"
#include "rect.h"
#include "ca_layer.h"
#include "label.h"
#include <stdbool.h>

__BEGIN_DECLS

typedef void (*mousedown_fp)(struct button* b);
typedef struct button {
	//common 
	Rect frame;
	char needs_redraw;
	ca_layer* layer;
	struct view* superview;
	mousedown_fp mousedown_handler;

	bool toggled;
	Label* label;
} Button;

Button* create_button(Rect frame, char* text);
void button_handle_click();

__END_DECLS

#endif
