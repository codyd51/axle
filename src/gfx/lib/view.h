#ifndef VIEW_H
#define VIEW_H

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct size {
	int w;
	int h;
} size;

typedef struct rect {
	coordinate origin;
	size size;
} rect;


typedef struct window {
	size size;
	uint32_t subviewsCount;
	struct view *subviews;
}

typedef struct view {
	rect frame;
	uint32_t zIndex;
	struct view *superview;
	uint32_t subviewsCount;
	struct view *subviews;
} view;

#endif
