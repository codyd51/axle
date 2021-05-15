#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#include <libamc/libamc.h>

#include <libgui/libgui.h>
#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/ca_layer.h>
#include <agx/lib/putpixel.h>

#include <awm/awm.h>

static Rect _content_view_sizer(gui_view_t* view, Size window_size) {
	return rect_make(point_zero(), window_size);
};

static void _place_circle(gui_view_t* view, Point mouse_pos) {
	Rect r = view->frame;
	printf("Content layer %d %d %d %d\n", r.origin.x, r.origin.y, r.size.width, r.size.height);
	gui_layer_draw_circle(view->content_layer, circle_make(mouse_pos, 20), color_white(), THICKNESS_FILLED);
}

static void _clear_background(gui_view_t* view, Point mouse_pos) {
	gui_layer_draw_rect(view->content_layer, rect_make(point_zero(), view->content_layer_frame.size), color_brown(), THICKNESS_FILLED);
}

int main(int argc, char** argv) {
	amc_register_service("com.user.paintbrush");

	gui_window_t* window = gui_window_create("Paintbrush", 600, 600);
	gui_view_t* content_view = gui_view_create(
		window,
		(gui_window_resized_cb_t)_content_view_sizer
	);
	_clear_background(content_view, point_zero());
	content_view->controls_content_layer = true;
	content_view->mouse_dragged_cb = (gui_mouse_dragged_cb_t)_place_circle;
	content_view->left_click_cb = (gui_mouse_left_click_cb_t)_place_circle;
	content_view->mouse_exited_cb = (gui_mouse_exited_cb_t)_clear_background;

	gui_enter_event_loop(window);

	return 0;
}

