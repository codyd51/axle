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
	struct view* view;
	mutable_array_t subwindows;
} Window;

typedef struct view {
	Rect frame;
	uint32_t zIndex;
	struct view *superview;
	mutable_array_t subviews;
} View;

void add_subview(View* view, View* subview); 
void add_subwindow(Window* window, Window* subwindow);

View* create_view(Rect frame);
Window* create_window(Rect frame);

#endif
