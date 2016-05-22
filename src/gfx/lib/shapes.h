#ifndef SHAPES_H
#define SHAPES_H

#include "gfx.h"

typedef struct line {
	Coordinate p1;
	Coordinate p2;
} Line;

typedef struct circle {
	Coordinate center;
	int radius;
} Circle;

typedef struct triangle {
	Coordinate p1;
	Coordinate p2;
	Coordinate p3;
} Triangle;

Coordinate create_coordinate(int x, int y);
Size create_size(int w, int h);
Rect create_rect(Coordinate origin, Size size);
Line create_line(Coordinate p1, Coordinate p2);
Circle create_circle(Coordinate center, int radius);
Triangle create_triangle(Coordinate p1, Coordinate p2, Coordinate p3);

#define THICKNESS_FILLED -1

void draw_rect(Screen* screen, Rect rect, int color, int thickness);
void draw_line(Screen* screen, Line line, int color, int thickness);
void draw_triangle(Screen* screen, Triangle triangle, int color, int thickness);
void draw_circle(Screen* screen, Circle circle, int color, int thickness);
#endif
