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

void normalize_coordinate(Screen* screen, Coordinate* p);

Coordinate point_make(int x, int y);
Size size_make(int w, int h);
Rect rect_make(Coordinate origin, Size size);
Line line_make(Coordinate p1, Coordinate p2);
Circle circle_make(Coordinate center, int radius);
Triangle triangle_make(Coordinate p1, Coordinate p2, Coordinate p3);

#define THICKNESS_FILLED -1

void draw_rect(Screen* screen, Rect rect, Color color, int thickness);
void draw_line(Screen* screen, Line line, Color color, int thickness);
void draw_triangle(Screen* screen, Triangle triangle, Color color, int thickness);
void draw_circle(Screen* screen, Circle circle, Color color, int thickness);
#endif
