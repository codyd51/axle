#ifndef VIEW_H
#define VIEW_H

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct Size {
	int width;
	int height;
} Size;

typedef struct rect {
	coordinate origin;
	Size size;
} rect;


typedef struct dynamic_array {
	type_t* array;
	uint32_t used;
	uint32_t size;
} dynamic_array;

typedef struct view {
	rect frame;
	uint32_t zIndex;
	struct view superview;
	dynamic_array *subviews;
} view;

typedef struct window {
	Size size;
	dynamic_array *subwindows;
	struct view view;
} window;

#endif
