#ifndef VIEW_H
#define VIEW_H

#include <std/std_base.h>
#include <stdint.h>
#include <std/array_m.h>
#include "color.h"

__BEGIN_DECLS

typedef struct coordinate {
	int x;
	int y;
} Coordinate;

typedef struct size {
	int width;
	int height;
} Size;

typedef struct rect {
	Coordinate origin;
	Size size;
} Rect;

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

typedef struct image {
	//common
	Rect frame;
	char needs_redraw;
	struct view* superview;

	uint32_t* bitmap;
} Image;

typedef struct view {
	//common
	Rect frame;
	char needs_redraw;
	struct view *superview;
	array_m* subviews;
	
	Color background_color;
	array_m* labels;
	array_m* images;
	array_m* bmps;
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

	Color** raw;
} Bmp;

Label* create_label(Rect frame, char* text);
Image* create_image(Rect frame, uint32_t* bitmap);
View* create_view(Rect frame);
Window* create_window(Rect frame);
Button* create_button(Rect frame, char* text);
Bmp* create_bmp(Rect frame, Color** raw);

void add_subview(View* view, View* subview);
void remove_subview(View* view, View* subview);
void add_sublabel(View* view, Label* label);
void remove_sublabel(View* view, Label* sublabel);
void add_subimage(View* view, Image* image);
void remove_subimage(View* view, Image* image);
void add_bmp(View* view, Bmp* bmp);
void remove_bmp(View* view, Bmp* bmp);

void add_subwindow(Window* window, Window* subwindow);
void remove_subwindow(Window* window, Window* subwindow);

void set_background_color(View* view, Color color);
void set_border_width(Window* window, int width);
void set_frame(View* view, Rect frame);
	
__END_DECLS

#endif
