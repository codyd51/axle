#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "../math.h"
#include "../lib/gfx.h"
#include "../lib/putpixel.h"
#include "color.h"

#include "shapes.h"

static void draw_rect_int(ca_layer* layer, Rect rect, Color color);

//convenience functions to make life easier
double line_length(Line line) {
	//distance formula
	return sqrt(pow(line.p2.x - line.p1.x, 2) + pow(line.p2.y - line.p1.y, 2));
}

Point line_center(Line line) {
	//average coordinates together
	double x = (line.p1.x + line.p2.x) / 2;
	double y = (line.p1.y + line.p2.y) / 2;
	return point_make(x, y);
}

Point triangle_center(Triangle t) {
	//average coordinates together
	double x = (t.p1.x + t.p2.x + t.p3.x) / 3;
	double y = (t.p1.y + t.p2.y + t.p3.y) / 3;
	return point_make(x, y);
}

Line line_make(Point p1, Point p2) {
	Line line;
	line.p1 = p1;
	line.p2 = p2;
	return line;
}

Circle circle_make(Point center, int radius) {
	Circle circle;
	circle.center = center;
	circle.radius = radius;
	return circle;
}

Triangle triangle_make(Point p1, Point p2, Point p3) {
	Triangle triangle;
	triangle.p1 = p1;
	triangle.p2 = p2;
	triangle.p3 = p3;
	return triangle;
}

void normalize_coordinate(ca_layer* layer, Point* p) {
	//don't try to write anywhere outside screen bounds
	p->x = MAX(p->x, 0);
	p->y = MAX(p->y, 0);
	p->x = MIN(p->x, layer->size.width);
	p->y = MIN(p->y, layer->size.height);
}

static void _fill_layer(ca_layer* layer, Color color) {
	// https://stackoverflow.com/questions/3345042/how-to-memset-memory-to-a-certain-pattern-instead-of-a-single-byte
	uint32_t fill_color = (color.val[2] << 16 | color.val[1] << 8 | color.val[0] << 0);

	uint32_t bytes_in_layer = layer->size.width * layer->size.height * gfx_bytes_per_pixel();
	uint32_t block_size = sizeof(fill_color);
	memmove(layer->raw, &fill_color, block_size);

	char* start = layer->raw;
	char* current = start + block_size;
	char* end = start + bytes_in_layer;

	while (current + block_size < end) {
		memmove(current, start, block_size);
		current += block_size;
		block_size *= 2;
	}
	// Fill the final half
	memmove(current, start, (int)end - (int)current);
}

//functions to draw shape structures
static void draw_rect_int_fast(ca_layer* layer, Rect rect, Color color) {
	// Filling a layer is a special case that can be quickly handled by memset()
	if (rect.origin.x == 0 &&
		rect.origin.y == 0 &&
		rect.size.width == layer->size.width &&
		rect.size.height == layer->size.height) {
		_fill_layer(layer, color);
		return;
	}

	//make sure we don't try to write to an invalid location
	if (rect.origin.x < 0) {
		rect.size.width += rect.origin.x;
		rect.origin.x = 0;
	}
	if (rect.origin.y < 0) {
		rect.size.height += rect.origin.y;
		rect.origin.y = 0;
	}
	if (rect.origin.x + rect.size.width >= layer->size.width) {
		rect.size.width -= (rect.origin.x + rect.size.width - layer->size.width);
	}
	if (rect.origin.y + rect.size.height >= layer->size.height) {
		rect.size.height -= (rect.origin.y + rect.size.height - layer->size.height);
	}

	int bpp = gfx_bytes_per_pixel();
	int offset = (rect.origin.x * bpp) + (rect.origin.y * layer->size.width * bpp);
	// TODO(PT): This will need to change if our BPP isn't exactly 4
	uint32_t fill_color = (color.val[2] << 16 | color.val[1] << 8 | color.val[0] << 0);
	uint32_t* where = (uint32_t*)(layer->raw + offset);
	for (int i = 0; i < rect.size.height; i++) {
		for (int j = 0; j < rect.size.width; j++) {
			where[j] = fill_color;
		}
		where += layer->size.width;
	}
}

void draw_rect(ca_layer* layer, Rect r, Color color, int thickness) {
	if (thickness == 0) return;

	int max_thickness = (MIN(r.size.width, r.size.height)) / 2;

	//if thickness is negative, fill the shape
	if (thickness < 0) thickness = max_thickness;

	//make sure they don't request a thickness too big
	thickness = MIN(thickness, max_thickness);
	//a filled shape is a special case that can be drawn faster
	if (thickness == max_thickness) {
		draw_rect_int_fast(layer, r, color);
		return;
	}

	//make sure we don't try to write to an invalid location
	if (r.origin.x < 0) {
		r.size.width += r.origin.x;
		r.origin.x = 0;
	}
	if (r.origin.y < 0) {
		r.size.height += r.origin.y;
		r.origin.y = 0;
	}
	if (r.origin.x + r.size.width >= layer->size.width) {
		r.size.width -= (r.origin.x + r.size.width - layer->size.width);
	}
	if (r.origin.y + r.size.height >= layer->size.height) {
		r.size.height -= (r.origin.y + r.size.height - layer->size.height);
	}

	int bpp = gfx_bytes_per_pixel();
	int offset = (r.origin.x * bpp) + (r.origin.y * layer->size.width * bpp);
	// TODO(PT): This will need to change if our BPP isn't exactly 4
	uint32_t fill_color = (color.val[2] << 16 | color.val[1] << 8 | color.val[0] << 0);
	uint32_t* where = (uint32_t*)(layer->raw + offset);

	// Top edge
	for (int i = 0; i < thickness; i++) {
		for (int j = 0; j < r.size.width; j++) {
			where[j] = fill_color;
		}
		where += layer->size.width;
	}

	// Left edge
	where = (uint32_t*)(layer->raw + offset);
	for (int i = 0; i < r.size.height; i++) {
		for (int j = 0; j < thickness; j++) {
			where[j] = fill_color;
		}
		where += layer->size.width;
	}

	// Right edge
	offset = ((r.origin.x + r.size.width - thickness) * bpp) + (r.origin.y * layer->size.width * bpp);
	where = (uint32_t*)(layer->raw + offset);
	for (int i = 0; i < r.size.height; i++) {
		for (int j = 0; j < thickness; j++) {
			where[j] = fill_color;
		}
		where += layer->size.width;
	}

	// Bottom edge
	offset = (r.origin.x * bpp) + ((r.origin.y + r.size.height - thickness) * layer->size.width * bpp);
	where = (uint32_t*)(layer->raw + offset);
	for (int i = 0; i < thickness; i++) {
		for (int j = 0; j < r.size.width; j++) {
			where[j] = fill_color;
		}
		where += layer->size.width;
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void draw_hline_fast(ca_layer* layer, Line line, Color color, int thickness) {
	//don't try to write anywhere outside screen bounds
	/*
	normalize_coordinate(layer, &line.p1);
	normalize_coordinate(layer, &line.p2);
	*/

	int bpp = gfx_bytes_per_pixel();

	//calculate starting point
	int offset = (line.p1.x * bpp) + (line.p1.y * bpp * layer->size.width);
	int length = line.p2.x - line.p1.x;
	int overhang = length + line.p1.x - layer->size.width;
	if (overhang > 0) {
		length -= overhang;
	}

	// TODO(PT): This will need to change if our BPP isn't exactly 4
	uint32_t fill_color = (color.val[2] << 16 | color.val[1] << 8 | color.val[0] << 0);
	uint32_t* where = layer->raw + offset;
	for (int i = 0; i < length; i++) {
		where[i] = fill_color;
	}
}

void draw_vline_fast(ca_layer* layer, Line line, Color color, int thickness) {
	//don't try to write anywhere outside screen bounds
	/*
	normalize_coordinate(layer, &line.p1);
	normalize_coordinate(layer, &line.p2);
	*/

	int bpp = gfx_bytes_per_pixel();

	//calculate starting point
	int offset = (line.p1.x * bpp) + (line.p1.y * bpp * layer->size.width);

	// TODO(PT): This will need to change if our BPP isn't exactly 4
	uint32_t fill_color = (color.val[2] << 16 | color.val[1] << 8 | color.val[0] << 0);
	uint32_t len = line.p2.y - line.p1.y;
	uint32_t* where = layer->raw + offset;
	for (int i = 0; i < len; i++) {
		where[offset] = fill_color;
		where += layer->size.width * bpp;
	}
}
#pragma GCC diagnostic pop

static void _draw_subline(ca_layer* layer, Line* line, Color color, int thickness) {
	int t;
	int distance;
	int xerr = 0, yerr = 0, delta_x, delta_y;
	int incx, incy;

	//get relative distances in both directions
	delta_x = line->p2.x - line->p1.x;
	delta_y = line->p2.y - line->p1.y;

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

	if (distance >= layer->size.width || distance >= layer->size.height) {
		Line l = *line;
		printf("Discarding invalid line draw:  (%d, %d) to (%d, %d)\n", l.p1.x, l.p1.y, l.p2.x, l.p2.y);
		return;
	}

	//draw line
	int curr_x = line->p1.x;
	int curr_y = line->p1.y;
	// TODO(PT): Investigate the circumstances where an extremely large distance is passed in, and fix the caller.
	for (t = 0; t < min(1000, distance + 1); t++) {
		putpixel(layer, curr_x, curr_y, color);

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

void draw_line(ca_layer* layer, Line line, Color color, int thickness) {
	//first things first
	//ensure we never try to draw outside screen bounds
	//normalize line coordinates
	line.p1.x = MAX(thickness / 2, line.p1.x);
	line.p1.y = MAX(thickness / 2, line.p1.y);
	line.p2.x = MIN(layer->size.width - (thickness / 2), line.p2.x);
	line.p2.y = MIN(layer->size.height - (thickness / 2), line.p2.y);

	//if the line is perfectly vertical or horizontal, this is a special case
	//that can be drawn much faster
	if (line.p1.x == line.p2.x) {
		////printf("draw_v")
		//draw_vline_fast(layer, line, color, thickness);
		//return;
	}
	else if (line.p1.y == line.p2.y) {
		draw_hline_fast(layer, line, color, thickness);
		return;
	}

	int off = thickness / 2;
	for (int i = 0; i < thickness; i++) {
		Line subline = line;
		subline.p1.x = subline.p1.x - off + i;
		subline.p2.x = subline.p2.x - off + i;
		_draw_subline(layer, &subline, color, thickness);
	}
}

void draw_triangle_int_fast(ca_layer* layer, Triangle triangle, Color color) {
	//bounding rectangle
	Point min;
	Point max;
	min.x = MIN(triangle.p1.x, triangle.p2.x);
	min.x = MIN(min.x, triangle.p3.x);
	min.y = MIN(triangle.p1.y, triangle.p2.y);
	min.y = MIN(min.y, triangle.p3.y);
	max.x = MAX(triangle.p1.x, triangle.p2.x);
	max.x = MAX(max.x, triangle.p3.x);
	max.y = MAX(triangle.p1.y, triangle.p2.y);
	max.y = MAX(max.y, triangle.p3.y);

	//scan bounding rectangle
	for (int y = min.y; y < max.y; y++) {
		for (int x = min.x; x < max.x; x++) {
			//the pixel is bounded by the rectangle if all half-space functions are positive
			if ((triangle.p1.x - triangle.p2.x) * (y - triangle.p1.y) - (triangle.p1.y - triangle.p2.y) * (x - triangle.p1.x) > 0 &&
			    (triangle.p2.x - triangle.p3.x) * (y - triangle.p2.y) - (triangle.p2.y - triangle.p3.y) * (x - triangle.p2.x) > 0 &&
			    (triangle.p3.x - triangle.p1.x) * (y - triangle.p3.y) - (triangle.p3.y - triangle.p1.y) * (x - triangle.p3.x) > 0) {
				putpixel(layer, x, y, color);
			}
		}
	}
}

void draw_triangle_int(ca_layer* layer, Triangle triangle, Color color) {
	Line l1 = line_make(triangle.p1, triangle.p2);
	Line l2 = line_make(triangle.p2, triangle.p3);
	Line l3 = line_make(triangle.p3, triangle.p1);

	draw_line(layer, l1, color, 1);
	draw_line(layer, l2, color, 1);
	draw_line(layer, l3, color, 1);
}

void draw_triangle(ca_layer* layer, Triangle tri, Color color, int thickness) {
	if (thickness == THICKNESS_FILLED) {
		draw_triangle_int_fast(layer, tri, color);
		return;
	}

	draw_triangle_int(layer, tri, color);
}

void draw_circle_int(ca_layer* layer, Circle circle, Color color) {
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

		putpixel(layer, circle.center.x + x, circle.center.y + y, color);
		putpixel(layer, circle.center.x - x, circle.center.y + y, color);
		putpixel(layer, circle.center.x + x, circle.center.y - y, color);
		putpixel(layer, circle.center.x - x, circle.center.y - y, color);
		putpixel(layer, circle.center.x + y, circle.center.y + x, color);
		putpixel(layer, circle.center.x - y, circle.center.y + x, color);
		putpixel(layer, circle.center.x + y, circle.center.y - x, color);
		putpixel(layer, circle.center.x - y, circle.center.y - x, color);
	} while (x < y);

	//put pixels at intersections of quadrants
	putpixel(layer, circle.center.x, circle.center.y - circle.radius, color);
	putpixel(layer, circle.center.x + circle.radius, circle.center.y, color);
	putpixel(layer, circle.center.x, circle.center.y + circle.radius, color);
	putpixel(layer, circle.center.x - circle.radius, circle.center.y, color);
}

void draw_circle(ca_layer* layer, Circle circ, Color color, int thickness) {
	if (circ.center.x + circ.radius > layer->size.width) {
		circ.radius = layer->size.width - circ.center.x;
	}
	if (circ.center.y + circ.radius > layer->size.height) {
		circ.radius = layer->size.height - circ.center.y;
	}

	int max_thickness = circ.radius;
	//if the thickness indicates the shape should be filled, set it as such
	if (thickness < 0) thickness = max_thickness;

	//make sure they don't set one too big
	thickness = MIN(thickness, max_thickness);

	// Filled circle?
	if (thickness == max_thickness) {
		for (int32_t y = -circ.radius; y <= circ.radius; y++) {
			for (int32_t x = -circ.radius; x <= circ.radius; x++) {
				if ((x*x) + (y*y) <= (circ.radius * circ.radius)) {
					putpixel(layer, circ.center.x + x, circ.center.y + y, color);
				}
			}
		}
		return;
	}

	Circle c = circle_make(circ.center, circ.radius);

	for (int i = 0; i <= thickness; i++) {
		draw_circle_int(layer, c, color);

		//decrease radius for next shell
		c.radius -= 1;
	}
}
