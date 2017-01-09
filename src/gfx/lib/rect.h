#ifndef RECT_H
#define RECT_H

#include "point.h"
#include "size.h"
#include <std/array_m.h>
#include <stdbool.h>

#define rect_min_x(r) ((r).origin.x)
#define rect_min_y(r) ((r).origin.y)
#define rect_max_x(r) ((r).origin.x + (r).size.width)
#define rect_max_y(r) ((r).origin.y + (r).size.height)

typedef struct rect {
	Point origin;
	Size size;
} Rect;

Rect rect_make(Point origin, Size size);
Rect rect_zero();

bool rect_intersects(Rect A, Rect B);

//explode subject rect into array of contiguous rects which are
//not occluded by cutting rect
Rect* rect_clip(Rect subject, Rect cutting);

//find the intersecting rect of a and b
Rect rect_intersect(Rect a, Rect b);
//returns true if point is bounded by rect
bool rect_contains_point(Rect r, Point p);

//convert inner to outer's coordinate space
Rect convert_rect(Rect outer, Rect inner);

#endif
