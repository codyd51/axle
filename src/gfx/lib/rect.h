#ifndef RECT_H
#define RECT_H

#include "point.h"
#include "size.h"

#define rect_min_x(r) ((r).origin.x)
#define rect_min_y(r) ((r).origin.y)
#define rect_max_x(r) ((r).origin.x + (r).size.width)
#define rect_max_y(r) ((r).origin.y + (r).size.height)

typedef struct rect {
	Coordinate origin;
	Size size;
} Rect;

Rect rect_make(Coordinate origin, Size size);
Rect rect_zero();

#endif
