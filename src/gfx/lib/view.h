#ifndef VIEW_H
#define VIEW_H

#include <std/std_base.h>
#include <stdint.h>
#include <std/array_m.h>
#include "color.h"
#include "rect.h"

__BEGIN_DECLS

typedef struct window {
	//common
	Rect frame;
	char needs_redraw;
	struct window* superview;
	array_m* subviews;

	Size size;
	char* title;
	struct view* title_view;
	struct view* content_view;
	Color border_color;
	int border_width;
} Window;

//TODO make proper subclass (c++?)
//Label is a type of View
typedef struct label {
	//common
	Rect frame;
	char needs_redraw;
	struct view* superview;

	char* text;
	Color text_color;
} Label;

typedef struct view {
	//common
	Rect frame;
	char needs_redraw;
	struct view *superview;
	array_m* subviews;
	
	Color background_color;
	array_m* labels;
	array_m* bmps;
	array_m* shaders;
} View;

typedef struct button {
	//common 
	Rect frame;
	char needs_redraw;
	struct view* superview;

	char* text;
	Color text_color;
} Button;

typedef struct bitmap {
	//common
	Rect frame;
	char needs_redraw;
	struct view* superview;

	Color* raw;
	Size raw_size;
} Bmp;

Label* create_label(Rect frame, char* text);
View* create_view(Rect frame);
Window* create_window(Rect frame);
Button* create_button(Rect frame, char* text);

Bmp* create_bmp(Rect frame, Color* raw);
Bmp* load_bmp(Rect frame, char* filename);

void add_subview(View* view, View* subview);
void remove_subview(View* view, View* subview);
void add_sublabel(View* view, Label* label);
void remove_sublabel(View* view, Label* sublabel);
void add_bmp(View* view, Bmp* bmp);
void remove_bmp(View* view, Bmp* bmp);

void add_subwindow(Window* window, Window* subwindow);
void remove_subwindow(Window* window, Window* subwindow);

void set_background_color(View* view, Color color);
void set_border_width(Window* window, int width);
void set_frame(View* view, Rect frame);
	
__END_DECLS

#endif
