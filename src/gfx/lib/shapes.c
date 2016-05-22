#include "shapes.h"
#include "gfx.h"
#include <std/math.h>

//convenience functions to make life easier
double line_length(line line) {
	//distance formula
	return sqrt(pow(line.p2.x - line.p1.x, 2) + pow(line.p2.y - line.p1.y, 2));
}

coordinate line_center(line line) {
	//average coordinates together
	double x = (line.p1.x + line.p2.x) / 2;
	double y = (line.p1.y + line.p2.y) / 2;
	return create_coordinate(x, y);
}

coordinate triangle_center(triangle t) {
	//average coordinates together
	double x = (t.p1.x + t.p2.x + t.p3.x) / 3;
	double y = (t.p1.y + t.p2.y + t.p3.y) / 3;
	return create_coordinate(x, y);
}

//functions to create shape structures
coordinate create_coordinate(int x, int y) {
	coordinate coord;
	coord.x = x;
	coord.y = y;
	return coord;
}

size create_size(int w, int h) {
	size size;
	size.width = w;
	size.height = h;
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

//functions to draw shape structures
static void draw_rect_int_fast(screen_t* screen, rect rect, int color) {
	for (int y = rect.origin.y; y < rect.origin.y + rect.size.height; y++) {
		for (int x = rect.origin.x; x < rect.origin.x + rect.size.width; x++) {
			putpixel(screen, x, y, color);
		}
	}
}

static void draw_rect_int(screen_t* screen, rect rect, int color) {
	line h1 = create_line(rect.origin, create_coordinate(rect.origin.x + rect.size.width, rect.origin.y));
	line h2 = create_line(create_coordinate(rect.origin.x, rect.origin.y + rect.size.height), create_coordinate(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height));
	line v1 = create_line(rect.origin, create_coordinate(rect.origin.x, rect.origin.y + rect.size.height));
	line v2 = create_line(create_coordinate(rect.origin.x + rect.size.width, rect.origin.y), create_coordinate(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height + 1));

	draw_line(screen, h1, color, 1);
	draw_line(screen, h2, color, 1);
	draw_line(screen, v1, color, 1);
	draw_line(screen, v2, color, 1);
}

void draw_rect(screen_t* screen, rect r, int color, int thickness) {
	int max_thickness = (MIN(r.size.width, r.size.height)) / 2;

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
	int w = r.size.width;
	int h = r.size.height;
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

void draw_hline_fast(screen_t* screen, line line, int color, int thickness) {
	for (int i = 0; i < thickness; i++) {
		//calculate starting point
		//increment y for next thickness since this line is horizontal
		uint16_t loc = ((line.p1.y + i) * screen->window.size.width) + line.p1.x;
		for (int j = 0; j < (line.p2.x - line.p1.x); j++) {
			screen->vmem[loc + j] = color;
		}
	}
}

void draw_vline_fast(screen_t* screen, line line, int color, int thickness) {
	for (int i = 0; i < thickness; i++) {
		//calculate starting point
		//increment x for next thickness since line is vertical
		uint16_t loc = (line.p1.y * screen->window.size.width) + (line.p1.x + i);
		for (int j = 0; j < (line.p2.y - line.p1.y); j++) {
			screen->vmem[loc + (j * screen->window.size.width)] = color;	
		}
	}
}

void draw_line(screen_t* screen, line line, int color, int thickness) {
	//if the line is perfectly vertical or horizontal, this is a special case
	//that can be drawn much faster
	if (line.p1.x == line.p2.x) {
		draw_vline_fast(screen, line, color, thickness);
		return;
	}
	else if (line.p1.y == line.p2.y) {
		draw_hline_fast(screen, line, color, thickness);
		return;
	}
	
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
	delta_x = abs(delta_x);
	delta_y = abs(delta_y);

	distance = MAX(delta_x, delta_y);

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

void draw_triangle_int(screen_t* screen, triangle triangle, int color) {
	line l1 = create_line(triangle.p1, triangle.p2);
	line l2 = create_line(triangle.p2, triangle.p3);
	line l3 = create_line(triangle.p3, triangle.p1);

	draw_line(screen, l1, color, 1);
	draw_line(screen, l2, color, 1);
	draw_line(screen, l3, color, 1);
}

void draw_triangle(screen_t* screen, triangle tri, int color, int thickness) {
	draw_triangle_int(screen, tri, color);
	return;

	//TODO fix implementation below
	
	//the max thickness of a triangle is the shortest distance
	//between the center and a vertice
	coordinate center = triangle_center(tri);
	double l1 = line_length(create_line(center, line_center(create_line(tri.p1, tri.p2))));
	double l2 = line_length(create_line(center, line_center(create_line(tri.p2, tri.p3))));
	double l3 = line_length(create_line(center, line_center(create_line(tri.p3, tri.p1))));

	double shortest_line = MIN(l1, l2);
	shortest_line = MIN(shortest_line, l3);

	int max_thickness = shortest_line;

	//if thickness indicates shape should be filled, set to max_thickness
	if (thickness < 0) thickness = max_thickness;

	//make sure thickness isn't too big
	thickness = MIN(thickness, max_thickness);

	printf_info("max_thickness: %d", max_thickness);
	printf_info("thickness: %d", thickness);
	printf_info("center.x: %d", center.x);
	printf_info("center.y: %d", center.y);

	coordinate p1 = tri.p1;
	coordinate p2 = tri.p2;
	coordinate p3 = tri.p3;	

	for (int i = 0; i < thickness; i++) {
		draw_triangle_int(screen, create_triangle(p1, p2, p3), color);

		//shrink for next shell
		p1.y += 1;
		p2.x += 1;
		p2.y -= 1;
		p3.x -= 1;
		p3.y -= 1;
	}
}

void draw_circle_int(screen_t* screen, circle circle, int color) {
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

void draw_circle(screen_t* screen, circle circ, int color, int thickness) {
	int max_thickness = circ.radius;
	
	//if the thickness indicates the shape should be filled, set it as such
	if (thickness < 0) thickness = max_thickness;

	//make sure they don't set one too big
	thickness = MIN(thickness, max_thickness);

	circle c = create_circle(circ.center, circ.radius);

	for (int i = 0; i <= thickness; i++) {
		draw_circle_int(screen, c, color);

		//decrease radius for next shell
		c.radius -= 1;
	}
}

