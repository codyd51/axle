#ifndef BUTTON_H
#define BUTTON_H

#include <std/std_base.h>
#include <stdint.h>
#include "gfx.h"
#include "color.h"
#include "rect.h"
#include "ca_layer.h"
#include "label.h"
#include <stdbool.h>

__BEGIN_DECLS

typedef struct button {
	//common 
	Rect frame;
	char needs_redraw;
	ca_layer* layer;
	struct view* superview;
	event_handler mousedown_handler;
	event_handler mouseup_handler;

	bool toggled;
	Label* label;
} Button;

Button* create_button(Rect frame, char* text);
void draw_button(ca_layer* dest, Button* button);

void button_handle_click();

//internal functions to process clicks on buttons
//should only be used by window manager to inform button of state change
void button_handle_mousedown(Button* button);
void button_handle_mouseup(Button* button);

__END_DECLS

#endif
