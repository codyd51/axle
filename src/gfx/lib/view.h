#ifndef VIEW_H
#define VIEW_H

#include <std/common.h>
#include <std/mutable_array.h>
#include "color.h"

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
	mutable_array_t subwindows;

	Size size;
	char* title;
	struct view* title_view;
	struct view* content_view;
	Color border_color;
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
	mutable_array_t subviews;
	
	Color background_color;
	mutable_array_t labels;
	mutable_array_t images;
} View;

void add_subview(View* view, View* subview);
void remove_subview(View* view, View* subview);
void add_sublabel(View* view, Label* label);
void remove_sublabel(View* view, Label* sublabel);
void add_subimage(View* view, Image* image);
void remove_subimage(View* view, Image* image);

void add_subwindow(Window* window, Window* subwindow);
void remove_subwindow(Window* window, Window* subwindow);

Label* create_label(Rect frame, char* text);
Image* create_image(Rect frame, uint32_t* bitmap);
View* create_view(Rect frame);
Window* create_window(Rect frame);

#endif
