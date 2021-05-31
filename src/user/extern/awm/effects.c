#include <agx/lib/putpixel.h>

#include "effects.h"

Color transcolor(Color c1, Color c2, float d) {
	if (d < 0) d = 0;
	if (d > 1) d = 1;
	return color_make(
		(c1.val[0] * (1 - d)) + (c2.val[0] * d),
		(c1.val[1] * (1 - d)) + (c2.val[1] * d),
		(c1.val[2] * (1 - d)) + (c2.val[2] * d)
	);
}

float pifdist(int x1, int y1, int x2, int y2) {
	float x = x1 - x2;
	float y = y1 - y2;
	return sqrt(x * x + y * y);
}

void radial_gradiant(ca_layer* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r) {
	int x_step = gradient_size.width / 200.0;
	int y_step = gradient_size.height / 200.0;
    if (x_step < 1) x_step = 1;
    if (y_step < 1) y_step = 1;
	for (uint32_t y = 0; y < gradient_size.height; y += y_step) {
		for (uint32_t x = 0; x < gradient_size.width; x += x_step) {
			Color c = transcolor(c1, c2, pifdist(x1, y1, x, y) / r);
			for (int i = 0; i < x_step; i++) {
				for (int j = 0; j < y_step; j++) {
					putpixel(layer, x+i, y+j, c);
				}
			}
		}
	}
}
#include <agx/lib/shapes.h>
#include <agx/lib/rect.h>

void draw_window_backdrop_segments(ca_layer* dest, user_window_t* window, int segments) {
#define LEFT_EDGE 0x1
#define RIGHT_EDGE 0x2
#define BOTTOM_EDGE 0x4
	//draw gradient around bottom, left, and right edges of window
	//gives 'depth' to windows
	Rect win_frame = window->frame;
	Rect draw = window->frame;
	int max_dist = 6;
	draw.origin.x -= max_dist;
	draw.size.width += max_dist * 2;
	draw.size.height += max_dist;

	Color col = color_black();
	//maximum alpha [0-255]
	int darkest = 200;

	for (int y = rect_min_y(draw); y < rect_max_y(draw); y++) {
		//past bottom of window?
		//only do bottom edge if that option is set
		/*
		if (!(segments & BOTTOM_EDGE)) {
			continue;
		}
		*/

		if (y < rect_min_y(win_frame) || y >= rect_max_y(win_frame)) {
			for (int x = rect_min_x(draw); x < rect_max_x(draw); x++) {
				int alpha = 0;
				//deal with corners
				if (x < rect_min_x(win_frame) || x >= rect_max_x(win_frame)) {
					int x_dist = 0;
					if (x < rect_min_x(win_frame)) {
						//left corner
						x_dist = rect_min_x(win_frame) - x;
					}
					else {
						//right corner
						x_dist = x - rect_max_x(win_frame);
					}

					int y_dist = 0;
					if (y < rect_max_y(win_frame)) {
						y_dist = rect_min_y(win_frame) - y;
					}
					else {
						y_dist = y - rect_max_y(win_frame);
					}

					//distance formula to get distance from corner
					float fact = (x_dist*x_dist) + (y_dist*y_dist);
					float norm = sqrt(fact);
					//invert intensity
					norm = max_dist - norm;

					alpha = (darkest / max_dist) * (norm);
				}
				//non-corner
				else {
					int y_dist = 0;
					if (y < rect_min_y(win_frame)) {
						y_dist = rect_min_y(win_frame) - y;
					}
					else {
						y_dist = (y - rect_max_y(win_frame));
					}
					alpha = (darkest / max_dist) * (max_dist - y_dist);
				}
				putpixel_alpha(dest, x, y, col, alpha);
			}
		}
		else {
			if (segments & LEFT_EDGE) {
				//left edge of window
				for (int x = rect_min_x(draw); x < rect_min_x(win_frame); x++) {
					int dist = (x - rect_min_x(draw));
					int alpha = (darkest / max_dist) * dist;
					putpixel_alpha(dest, x, y, col, alpha);
				}
			}
			if (segments & RIGHT_EDGE) {
				//right edge of window
				for (int x = rect_max_x(win_frame); x < rect_max_x(draw); x++) {
					int dist = (max_dist - (x - rect_max_x(win_frame)));
					int alpha = (darkest / max_dist) * dist;
					putpixel_alpha(dest, x, y, col, alpha);
				}
			}
		}
	}
}
