#ifndef RECT_H
#define RECT_H

#include "point.h"
#include "size.h"

typedef struct rect {
	Coordinate origin;
	Size size;
} Rect;

Rect rect_make(Coordinate origin, Size size);
Rect rect_zero();

#endif
