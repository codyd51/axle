#include "rect.h"

Rect rect_make(Coordinate origin, Size size) {
	Rect rect;
	rect.origin = origin;
	rect.size = size;
	return rect;
}

Rect rect_zero() {
	return rect_make(point_zero(), size_zero());
}
