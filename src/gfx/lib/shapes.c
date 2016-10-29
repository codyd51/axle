#include "shapes.h"
#include "gfx.h"
#include <std/math.h>
#include "color.h"

//convenience functions to make life easier
double line_length(Line line) {
	//distance formula
	return sqrt(pow(line.p2.x - line.p1.x, 2) + pow(line.p2.y - line.p1.y, 2));
}

Coordinate line_center(Line line) {
	//average coordinates together
	double x = (line.p1.x + line.p2.x) / 2;
	double y = (line.p1.y + line.p2.y) / 2;
	return point_make(x, y);
}

Coordinate triangle_center(Triangle t) {
	//average coordinates together
	double x = (t.p1.x + t.p2.x + t.p3.x) / 3;
	double y = (t.p1.y + t.p2.y + t.p3.y) / 3;
	return point_make(x, y);
}

Line line_make(Coordinate p1, Coordinate p2) {
	Line line;
	line.p1 = p1;
	line.p2 = p2;
	return line;
}

Circle circle_make(Coordinate center, int radius) {
	Circle circle;
	circle.center = center;
	circle.radius = radius;
	return circle;
}

Triangle triangle_make(Coordinate p1, Coordinate p2, Coordinate p3) {
	Triangle triangle;
	triangle.p1 = p1;
	triangle.p2 = p2;
	triangle.p3 = p3;
	return triangle;
}

void normalize_coordinate(ca_layer* layer, Coordinate* p) {
	//don't try to write anywhere outside screen bounds
	p->x = MAX(p->x, 0);
	p->y = MAX(p->y, 0);
	p->x = MIN(p->x, layer->size.width);
	p->y = MIN(p->y, layer->size.height);
}

//functions to draw shape structures
static void draw_rect_int_fast(ca_layer* layer, Rect rect, Color color) {
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
	
	bool rgb = (gfx_depth() == VESA_DEPTH);
	int bpp = (rgb ? 3 : 1);

	int offset = rect.origin.x * bpp + rect.origin.y * layer->size.width * bpp;
	int row_start = offset;
	for (int y = rect.origin.y; y < rect.origin.y + rect.size.height; y++) {
		for (int x = rect.origin.x; x < rect.origin.x + rect.size.width; x++) {
			if (rgb) {
				//we have to write the pixels in BGR, not RGB
				layer->raw[offset++] = color.val[2];
				layer->raw[offset++] = color.val[1];
				layer->raw[offset++] = color.val[0];
			}
			else {
				layer->raw[offset++] = color.val[0];
			}
		}
		//move down 1 row
		row_start += layer->size.width * bpp;
		offset = row_start;
	}
}

static void draw_rect_int(ca_layer* layer, Rect rect, Color color) {
	Line h1 = line_make(rect.origin, point_make(rect.origin.x + rect.size.width, rect.origin.y));
	Line h2 = line_make(point_make(rect.origin.x, rect.origin.y + rect.size.height), point_make(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height));
	Line v1 = line_make(rect.origin, point_make(rect.origin.x, rect.origin.y + rect.size.height));
	Line v2 = line_make(point_make(rect.origin.x + rect.size.width, rect.origin.y), point_make(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height + 1));

	draw_line(layer, h1, color, 1);
	draw_line(layer, h2, color, 1);
	draw_line(layer, v1, color, 1);
	draw_line(layer, v2, color, 1);
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
	
	int x = r.origin.x;
	int y = r.origin.y;
	int w = r.size.width;
	int h = r.size.height;
	for (int i = 0; i <= thickness; i++) {
		Coordinate origin = point_make(x, y);
		Size size = size_make(w, h);
		Rect rt = rect_make(origin, size);

		draw_rect_int(layer, rt, color);

		//decrement values for next shell
		x++;
		y++;
		w -= 2;
		h -= 2;
	}
}

void draw_hline_fast(ca_layer* layer, Line line, Color color, int thickness) {
	//don't try to write anywhere outside screen bounds
	normalize_coordinate(layer, &line.p1);
	normalize_coordinate(layer, &line.p2);

	bool rgb = (gfx_depth() == VESA_DEPTH);
	int bpp = (rgb ? 3 : 1);

	//calculate starting point
	int offset = (line.p1.x * bpp) + (line.p1.y * bpp * layer->size.width);
	for (int i = 0; i < line.p2.x - line.p1.x; i++) {
		if (rgb) {
			//we have to write the pixels in BGR, not RGB
			layer->raw[offset++] = color.val[2];
			layer->raw[offset++] = color.val[1];
			layer->raw[offset++] = color.val[0];
		}
		else {
			layer->raw[offset++] = color.val[0];
		}
	}
}

void draw_vline_fast(ca_layer* layer, Line line, Color color, int thickness) {
	//don't try to write anywhere outside screen bounds
	normalize_coordinate(layer, &line.p1);
	normalize_coordinate(layer, &line.p2);

	bool rgb = (gfx_depth() == VESA_DEPTH);
	int bpp = (rgb ? 3 : 1);

	//calculate starting point
	int offset = (line.p1.x * bpp) + (line.p1.y * bpp * layer->size.width);
	int row_start = offset;
	for (int i = 0; i < line.p2.y - line.p1.y; i++) {
		if (rgb) {
			//we have to write the pixels in BGR, not RGB
			layer->raw[offset++] = color.val[2];
			layer->raw[offset++] = color.val[1];
			layer->raw[offset] = color.val[0];
		}
		else {
			layer->raw[offset] = color.val[0];
		}
		//go to next row
		row_start += layer->size.width * bpp;
		offset = row_start;
	}
}

void draw_line(ca_layer* layer, Line line, Color color, int thickness) {
	//first things first
	//ensure we never try to draw outside screen bounds
	//normalize line coordinates
	line.p1.x = MAX(0, line.p1.x);
	line.p1.y = MAX(0, line.p1.y);
	line.p2.x = MIN(layer->size.width - 1, line.p2.x);
	line.p2.y = MIN(layer->size.height - 1, line.p2.y);

	//if the line is perfectly vertical or horizontal, this is a special case
	//that can be drawn much faster
	if (line.p1.x == line.p2.x) {
		draw_vline_fast(layer, line, color, thickness);
		return;
	}
	else if (line.p1.y == line.p2.y) {
		draw_hline_fast(layer, line, color, thickness);
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

void draw_triangle_int_fast(ca_layer* layer, Triangle triangle, Color color) {
	//bounding rectangle
	Coordinate min;
	Coordinate max;
	min.x = MIN(triangle.p1.x, triangle.p2.x);
	min.x = MIN(min.x, triangle.p3.x);
	min.y = MIN(triangle.p1.y, triangle.p2.y);
	min.y = MIN(min.y, triangle.p3.y);
	max.x = MAX(triangle.p1.x, triangle.p2.x);
	max.x = MAX(min.x, triangle.p3.x);
	max.y = MAX(triangle.p1.y, triangle.p2.y);
	max.y = MAX(min.y, triangle.p3.y);

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

Line shrink_line(Coordinate p1, Coordinate p2, float pixel_count) {
	//return line_make(point_make(p1.x - 5, p1.y - 5), l.p2);
	//if (p1.x == p2.x && p1.y == p2.y) return l;
	double dx = p2.x - p1.x;
	double dy = p2.y - p1.y;
	if (!dx) {
		//vertical line
		if (p2.y < p1.y) {
			p2.y -= pixel_count;
		}
		else {
			p2.y += pixel_count;
		}
	}
	else if (!dy) {
		//horizontal line
		if (p2.x < p1.x) {
			p2.x -= pixel_count;
		}
		else {
			p2.x += pixel_count;
		}
	}
	else {
		//diagonal line
		double length = sqrt(dx * dx + dy * dy);
		double scale = (length + pixel_count) / length;
		dx *= scale;
		dy *= scale;
		p2.x = (int)(p1.x + (int)dx);
		p2.y = (int)(p1.y + (int)dy);
	}
	return line_make(p1, p2);
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

	Circle c = circle_make(circ.center, circ.radius);

	for (int i = 0; i <= thickness; i++) {
		draw_circle_int(layer, c, color);

		//decrease radius for next shell
		c.radius -= 1;
	}
}

