#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#include <kernel/amc.h>
#include <libgui/libgui.h>
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>

Color transcolor(Color c1, Color c2, float d) {
	if (d < 0) d = 0;
	if (d > 1) d = 1;
	return color_make(
		(c1.val[0] * (1 - d)) + (c2.val[0] * d),
		(c1.val[1] * (1 - d)) + (c2.val[1] * d),
		(c1.val[2] * (1 - d)) + (c2.val[2] * d)
	);
}

// Replace it with another function if exists
float pifdist(int x1, int y1, int x2, int y2) {
	float x = x1 - x2;
	float y = y1 - y2;
	return sqrt(x * x + y * y);
}

void _radial_gradiant(gui_layer_t* layer, Size gradient_size, Color c1, Color c2, int x1, int y1, float r) {
	for (uint32_t y = 0; y < gradient_size.height; y++) {
		//printf("draw row %d\n", y);
		for (uint32_t x = 0; x < gradient_size.width; x++) {
			putpixel(layer->fixed_layer.inner, x, y, transcolor(c1, c2, pifdist(x1, y1, x, y) / r));
		}
	}
}

static Rect _content_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static void _redraw_gradient(gui_view_t* view) {
	static Color c1, c2;
	if (!c1.val[0] && !c1.val[1] && !c1.val[2]) {
		c1 = c2 = color_make(127, 127, 127);
	}
	int interval = 20;

	c1.val[0] += (-(interval/2)) + rand() % interval;
	c1.val[1] += (-(interval/2)) + rand() % interval;
	c1.val[2] += (-(interval/2)) + rand() % interval;

	c2.val[0] += (-(interval/2)) + rand() % interval;
	c2.val[1] += (-(interval/2)) + rand() % interval;
	c2.val[2] += (-(interval/2)) + rand() % interval;

	Rect r = rect_make(point_zero(), view->content_layer_frame.size);
	_radial_gradiant(
		view->content_layer,
		r.size,
		c1,
		c2, 
		r.size.width / 2.0,
		r.size.height / 2.0,
		r.size.height / 3.0
	);

	gui_timer_start(view->window, 50, (gui_timer_cb_t)_redraw_gradient, view);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.rainbow");

	gui_window_t* window = gui_window_create("Rainbow", 300, 300);
	gui_view_t* content_view = gui_view_create(
		window,
		(gui_window_resized_cb_t)_content_view_sizer
	);
	content_view->controls_content_layer = true;

	_redraw_gradient(content_view);

	gui_enter_event_loop(window);

	return 0;
}

