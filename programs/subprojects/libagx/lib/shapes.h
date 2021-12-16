#ifndef SHAPES_H
#define SHAPES_H

#include "point.h"
#include "size.h"
#include "ca_layer.h"
#include "color.h"

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

#define point_make(x, y) ((Point){x, y})
#define point_zero() ((Point){0, 0})
#define size_make(w, h) ((Size){w, h})
#define size_zero() ((Size){0, 0})
#define rect_make(o, s) ((Rect){o, s})
#define rect_zero() ((Rect){point_zero(), size_zero()})

void normalize_coordinate(ca_layer* layer, Point* p);

Line line_make(Point p1, Point p2);
Circle circle_make(Point center, int radius);
Triangle triangle_make(Point p1, Point p2, Point p3);

#define THICKNESS_FILLED -1

void draw_rect(ca_layer* layer, Rect rect, Color color, int thickness);
void draw_line(ca_layer* layer, Line line, Color color, int thickness);
void draw_triangle(ca_layer* layer, Triangle triangle, Color color, int thickness);
void draw_circle(ca_layer* layer, Circle circle, Color color, int thickness);

#endif
