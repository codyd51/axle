#include "point.h"

Coordinate point_make(int x, int y) {
	Coordinate coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

Coordinate point_zero() {
	return point_make(0, 0);
}
