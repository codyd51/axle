#ifndef VIEW_H
#define VIEW_H

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct size {
	int width;
	int height;
} size;

typedef struct rect {
	coordinate origin;
	size size;
} rect;


typedef struct window {
	struct size size;
	uint32_t subviewsCount;
	struct view *subviews;
} window;

typedef struct view {
	rect frame;
	uint32_t zIndex;
	struct view *superview;
	uint32_t subviewsCount;
	struct view *subviews;
} view;

#endif
