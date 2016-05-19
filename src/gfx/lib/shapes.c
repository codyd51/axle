#include "shapes.h"
#include "gfx.h"
#include <std/math.h>

coordinate create_coordinate(int x, int y) {
	coordinate coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

size create_size(int w, int h) {
	size size;
	size.w = w;
	size.h = h;
	return size;
}

rect create_rect(coordinate origin, size size) {
	rect rect;
	rect.origin = origin;
	rect.size = size;
	return rect;
}

line create_line(coordinate p1, coordinate p2) {
	line line;
	line.p1 = p1;
	line.p2 = p2;
	return line;
}

circle create_circle(coordinate center, int radius) {
	circle circle;
	circle.center = center;
	circle.radius = radius;
	return circle;
}

triangle create_triangle(coordinate p1, coordinate p2, coordinate p3) {
	triangle triangle;
	triangle.p1 = p1;
	triangle.p2 = p2;
	triangle.p3 = p3;
	return triangle;
}

static void draw_rect_int_fast(screen_t* screen, rect rect, int color) {
	int y = rect.origin.y;
	for (; y < rect.size.h; y++) {
		int x = rect.origin.x;
		for (; x < rect.size.w; x++) {
			putpixel(screen, x, y, color);
		}
	}
}

static void draw_rect_int(screen_t* screen, rect rect, int color) {
	line h1 = create_line(rect.origin, create_coordinate(rect.origin.x + rect.size.w, rect.origin.y));
	line h2 = create_line(create_coordinate(rect.origin.x, rect.origin.y + rect.size.h), create_coordinate(rect.origin.x + rect.size.w, rect.origin.y + rect.size.h));
	line v1 = create_line(rect.origin, create_coordinate(rect.origin.x, rect.origin.y + rect.size.h));
	line v2 = create_line(create_coordinate(rect.origin.x + rect.size.w, rect.origin.y), create_coordinate(rect.origin.x + rect.size.w, rect.origin.y + rect.size.h + 1));

	draw_line(screen, h1, color, 1);
	draw_line(screen, h2, color, 1);
	draw_line(screen, v1, color, 1);
	draw_line(screen, v2, color, 1);
}

void draw_rect(screen_t* screen, rect r, int color, int thickness) {
	int max_thickness = r.size.w / 2;

	//if thickness is negative, fill the shape
	if (thickness < 0) thickness = max_thickness;
	
	//make sure they don't request a thickness too big
	thickness = MIN(thickness, max_thickness);

	//a filled shape is a special case that can be drawn faster
	if (thickness == max_thickness) {
		draw_rect_int_fast(screen, r, color);
		return;
	}

	int x = r.origin.x;
	int y = r.origin.y;
	int w = r.size.w;
	int h = r.size.h;
	for (int i = 0; i <= thickness; i++) {
		coordinate origin = create_coordinate(x, y);
		size size = create_size(w, h);
		rect rt = create_rect(origin, size);

		draw_rect_int(screen, rt, color);

		//decrement values for next shell
		x++;
		y++;
		w -= 2;
		h -= 2;
	}
}

void draw_line(screen_t* screen, line line, int color, int thickness) {
	int t;
	int distance;
	int xerr = 0, yerr = 0, delta_x, delta_y;
	int incx, incy;

	//get relative distances in both directions
	delta_x = line.p2.x - line.p1.x;
	delta_y = line.p2.y - line.p1.y;

	//figure out direction of increment
	//incrememnt of 0 indicates either vertical or 
	//horizontal line
	if (delta_x > 0) incx = 1;
	else if (delta_x == 0) incx = 0;
	else incx = -1;

	if (delta_y > 0) incy = 1;
	else if (delta_y == 0) incy = 0;
	else incy = -1;

	//figure out which distance is greater
	//TODO implement abs
	if (delta_x < 0) delta_x = -delta_x;
	if (delta_y < 0) delta_y = -delta_y;

	if (delta_x > delta_y) distance = delta_x;
	else distance = delta_y;

	//draw line
	int curr_x = line.p1.x;
	int curr_y = line.p1.y;
	for (t = 0; t < distance + 1; t++) {
		putpixel(screen, curr_x, curr_y, color);

		xerr += delta_x;
		yerr += delta_y;
		if (xerr > distance) {
			xerr -= distance;
			curr_x += incx;
		}
		if (yerr > distance) {
			yerr -= distance;
			curr_y += incy;
		}
	}
}

double line_length(line line) {
	//distance formula
	return sqrt(pow(line.p2.x - line.p1.x, 2) + (line.p2.y - line.p1.y, 2));
}

void draw_triangle(screen_t* screen, triangle triangle, int color, int thickness) {
	line l1 = create_line(triangle.p1, triangle.p2);
	line l2 = create_line(triangle.p2, triangle.p3);
	line l3 = create_line(triangle.p3, triangle.p1);

	draw_line(screen, l1, color, 1);
	draw_line(screen, l2, color, 1);
	draw_line(screen, l3, color, 1);
}

void draw_circle(screen_t* screen, circle circle, int color, int thickness) {
	int x = 0;
	int y = circle.radius;
	int dp = 1 - circle.radius;
	do {
		if (dp < 0) {
			dp = dp + 2 * (++x) + 3;
		}
		else {
			dp = dp + 2 * (++x) - 2 * (--y) + 5;
		}

		putpixel(screen, circle.center.x + x, circle.center.y + y, color);
		putpixel(screen, circle.center.x - x, circle.center.y + y, color);
		putpixel(screen, circle.center.x + x, circle.center.y - y, color);
		putpixel(screen, circle.center.x - x, circle.center.y - y, color);
		putpixel(screen, circle.center.x + y, circle.center.y + x, color);
		putpixel(screen, circle.center.x - y, circle.center.y + x, color);
		putpixel(screen, circle.center.x + y, circle.center.y - x, color);
		putpixel(screen, circle.center.x - y, circle.center.y - x, color);
	} while (x < y);

	//put pixels at intersections of quadrants
	putpixel(screen, circle.center.x, circle.center.y - circle.radius, color);
	putpixel(screen, circle.center.x + circle.radius, circle.center.y, color);
	putpixel(screen, circle.center.x, circle.center.y + circle.radius, color);
	putpixel(screen, circle.center.x - circle.radius, circle.center.y, color);
}

