#ifndef VIEW_H
#define VIEW_H

#include <std/std_base.h>
#include <stdint.h>
#include <std/array_m.h>
#include "color.h"
#include "rect.h"
#include "bmp.h"
#include "label.h"
#include "window.h"
#include "button.h"

__BEGIN_DECLS

typedef struct view {
	//common
	Rect frame;
	char needs_redraw; 
	ca_layer* layer;
	struct view *superview;
	array_m* subviews;
	
	Color background_color;
	array_m* labels;
	array_m* bmps;
	array_m* shaders;
	array_m* buttons;
} View;

View* create_view(Rect frame);
void view_teardown(View* view);

void set_background_color(View* view, Color color);
void set_frame(View* view, Rect frame);
void set_alpha(View* view, float alpha);

void add_subview(View* view, View* subview);
void remove_subview(View* view, View* subview);

void add_sublabel(View* view, Label* label);
void remove_sublabel(View* view, Label* sublabel);

void add_bmp(View* view, Bmp* bmp);
void remove_bmp(View* view, Bmp* bmp);

void add_button(View* view, Button* button);
void remove_button(View* view, Button* button);

void mark_needs_redraw(View* view);
	
__END_DECLS

#endif
