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

/**
 * @brief Construct a Button container using a given size and label
 * @param frame The size of the Button to create
 * @param text The label to render to the button
 * @return The newly constructed Button
 */
Button* create_button(Rect frame, char* text);

/**
 * @brief Render the image represented by @p button to the graphical layer @p dest
 * @param dest The graphical layer to render to
 * @param button Button object used to render image
 */
void draw_button(ca_layer* dest, Button* button);


/**
 * @brief Internal function to process clicks on buttons
 * Forwards button event to button->mousedown_handler
 * @param button The Button whose delegate should be notified of the event
 */
void button_handle_mousedown(Button* button);

/**
 * @brief Internal function to process clicks on buttons
 * Forwards button event to button->mouseup_handler
 * @param button The Button whose delegate should be notified of the event
 */
void button_handle_mouseup(Button* button);

__END_DECLS

#endif
