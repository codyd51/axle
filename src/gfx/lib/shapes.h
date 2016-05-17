#ifndef SHAPES_H
#define SHAPES_H

#include "gfx.h"

typedef struct coordinate {
	int x;
	int y;
} coordinate;

typedef struct size {
	int w;
	int h;
} size;

typedef struct rect {
	coordinate origin;
	size size;
} rect;

typedef struct line {
	coordinate p1;
	coordinate p2;
} line;

typedef struct circle {
	coordinate center;
	int radius;
} circle;

typedef struct triangle {
	coordinate p1;
	coordinate p2;
	coordinate p3;
} triangle;

coordinate create_coordinate(int x, int y);
size create_size(int w, int h);
rect create_rect(coordinate origin, size size);
line create_line(coordinate p1, coordinate p2);
circle create_circle(coordinate center, int radius);
triangle create_triangle(coordinate p1, coordinate p2, coordinate p3);
void draw_rect(screen_t* screen, rect rect, int color);
void draw_line(screen_t* screen, line line, int color);
void draw_triangle(screen_t* screen, triangle triangle, int color);

#endif
