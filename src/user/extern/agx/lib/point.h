#ifndef POINT_H
#define POINT_H

typedef struct coordinate {
	int x;
	int y;
} Point;

Point point_make(int x, int y);
Point point_zero();

#endif
