#ifndef VIEW_H
#define VIEW_H

#include <std/common.h>
#include <std/mutable_array.h>

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
	struct view* title_view;
	struct view* content_view;
	uint32_t border_color;
	mutable_array_t subwindows;
} Window;

//TODO make proper subclass (c++?)
//Label is a type of View
typedef struct label {
	Rect frame;
	char* text;
	uint32_t text_color;
} Label;

typedef struct view {
	Rect frame;
	uint32_t zIndex;
	struct view *superview;
	uint32_t background_color;
	mutable_array_t subviews;
	mutable_array_t labels;
} View;

void add_subview(View* view, View* subview);
void add_sublabel(View* view, Label* label);
void add_subwindow(Window* window, Window* subwindow);

View* create_view(Rect frame);
Window* create_window(Rect frame);

#endif
