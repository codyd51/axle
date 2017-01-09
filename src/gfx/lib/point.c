#include "point.h"

Point point_make(int x, int y) {
	Point coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

Point point_zero() {
	return point_make(0, 0);
}
