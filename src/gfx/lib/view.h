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
	Size size;
	Rect frame;
	char* title;
	struct view* title_view;
	struct view* content_view;
	Color border_color;
	mutable_array_t subwindows;
} Window;

//TODO make proper subclass (c++?)
//Label is a type of View
typedef struct label {
	Rect frame;
	char* text;
	Color text_color;
} Label;

typedef struct image {
	Rect frame;
	uint32_t* bitmap;
} Image;

typedef struct view {
	Rect frame;
	struct view *superview;
	Color background_color;
	mutable_array_t subviews;
	mutable_array_t labels;
	mutable_array_t images;
} View;

void add_subview(View* view, View* subview);
void add_sublabel(View* view, Label* label);
void add_subimage(View* view, Image* image);

void add_subwindow(Window* window, Window* subwindow);

Label* create_label(Rect frame, char* text);
Image* create_image(Rect frame, uint32_t* bitmap);
View* create_view(Rect frame);
Window* create_window(Rect frame);

#endif
