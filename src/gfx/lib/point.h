#ifndef POINT_H
#define POINT_H

typedef struct coordinate {
	int x;
	int y;
} Coordinate;

Coordinate point_make(int x, int y);
Coordinate point_zero();

#endif
