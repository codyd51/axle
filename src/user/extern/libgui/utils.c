#include <string.h>
#include <stdio.h>

#include <agx/lib/shapes.h>

#include "libgui.h"
#include "utils.h"

void draw_diagonal_insets(gui_layer_t* layer, Rect outer, Rect inner, Color c, uint32_t width) {
	// Draw diagonal lines indicating an outset
	int t = width;

	// Top left corner
	Line l = line_make(
		point_make(
			outer.origin.x + (t/2),
			outer.origin.y
		),
		inner.origin
	);
	gui_layer_draw_line(layer, l, c, t);

	// Bottom left corner
	l = line_make(
		point_make(
			rect_min_x(outer) + (t/2),
			rect_max_y(outer) - (t/2)
		),
		point_make(
			rect_min_x(inner),
			rect_max_y(inner) - (t/2)
		)
	);
	gui_layer_draw_line(layer, l, c, t);

	// Top right corner
	l = line_make(
		point_make(
			rect_max_x(outer) - (t/2),
			rect_min_y(outer)
		),
		point_make(
			rect_max_x(inner),
			rect_min_y(inner)
		)
	);
	gui_layer_draw_line(layer, l, c, t);

	// Bottom right corner
	l = line_make(
		point_make(
			rect_max_x(outer) - (t/2),
			rect_max_y(outer) - (t/2)
		),
		point_make(
			rect_max_x(inner) - (t/2),
			rect_max_y(inner)
		)
	);
	gui_layer_draw_line(layer, l, c, t);
}

const char* rect_print(Rect r) {
	printf("{(%d, %d), (%d, %d)} - ", r.origin.x, r.origin.y, r.size.width, r.size.height);
	return "";
}
