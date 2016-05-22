#ifndef VIEW_H
#define VIEW_H

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
	uint32_t subviewsCount;
	struct view *subviews;
} Window;

typedef struct view {
	Rect frame;
	uint32_t zIndex;
	struct view *superview;
	uint32_t subviewsCount;
	struct view *subviews;
} View;

#endif
