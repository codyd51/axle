#ifndef VIEW_H
#define VIEW_H

#include "shapes.h"

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
