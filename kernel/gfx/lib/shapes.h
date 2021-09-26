#ifndef SHAPES_H
#define SHAPES_H

#include "gfx.h"

typedef struct line {
	Point p1;
	Point p2;
} Line;

typedef struct circle {
	Point center;
	int radius;
} Circle;

typedef struct triangle {
	Point p1;
	Point p2;
	Point p3;
} Triangle;

void normalize_coordinate(ca_layer* layer, Point* p);

Point point_make(int x, int y);
Size size_make(int w, int h);
Rect rect_make(Point origin, Size size);
Line line_make(Point p1, Point p2);
Circle circle_make(Point center, int radius);
Triangle triangle_make(Point p1, Point p2, Point p3);

#define THICKNESS_FILLED -1

void draw_rect(ca_layer* layer, Rect rect, Color color, int thickness);
void draw_line(ca_layer* layer, Line line, Color color, int thickness);
void draw_triangle(ca_layer* layer, Triangle triangle, Color color, int thickness);
void draw_circle(ca_layer* layer, Circle circle, Color color, int thickness);

#endif
