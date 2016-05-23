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


typedef struct view {
	rect frame;
	uint32_t zIndex;
	struct view *superview;
	uint32_t subviewsCount;
	struct view *subviews;
} view;

typedef struct window {
	Size size;
	uint32_t subwindow_count;
	struct window *subwindows;
	struct view view;
} window;

#endif
