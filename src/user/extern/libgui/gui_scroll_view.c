#include <stdio.h>
#include <string.h>
#include <agx/lib/shapes.h>
#include <agx/font/font.h>

#include "gui_scroll_view.h"
#include "libgui.h"

static void _noop() {}

void gui_scroll_view_alloc_dynamic_fields(gui_scroll_view_t* view) {
	view->content_layer = calloc(1, sizeof(gui_layer_t));
	view->content_layer->scroll_layer.type = GUI_SCROLL_LAYER;
	view->content_layer->scroll_layer.inner = ca_scrolling_layer_create(_gui_screen_resolution());
	view->subviews = array_create(32);
}

gui_scroll_view_t* gui_scroll_view_alloc(void) {
	gui_scroll_view_t* v = calloc(1, sizeof(gui_scroll_view_t));
	gui_scroll_view_alloc_dynamic_fields(v);
	return v;
}

static void _handle_mouse_scrolled(gui_scroll_view_t* view, int8_t delta_z) {
	view->content_layer->scroll_layer.inner->scroll_offset.height += delta_z * 10;
	view->content_layer->scroll_layer.inner->scroll_offset.height = max(0, view->content_layer->scroll_layer.inner->scroll_offset.height);
}

gui_elem_t* _gui_scroll_view_elem_for_mouse_pos(gui_scroll_view_t* sv, Point mouse_pos) {
	mouse_pos.x += sv->content_layer->scroll_layer.inner->scroll_offset.width;
	mouse_pos.y += sv->content_layer->scroll_layer.inner->scroll_offset.height;
	return gui_view_elem_for_mouse_pos(sv, mouse_pos);
}

static void _gui_scroll_view_fill_background(gui_scroll_view_t* sv, bool is_active) {
	gui_layer_draw_rect(
		sv->content_layer, 
		rect_make(
			point_make(
				sv->content_layer->scroll_layer.inner->scroll_offset.width,
				sv->content_layer->scroll_layer.inner->scroll_offset.height
			),
			sv->content_layer_frame.size
		), 
		sv->background_color, 
		THICKNESS_FILLED
	);
}

void gui_scroll_view_init(gui_scroll_view_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_view_init(view, window, sizer_cb);
	view->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_handle_mouse_scrolled;
    view->elem_for_mouse_pos_cb = (gui_view_elem_for_mouse_pos_cb_t)_gui_scroll_view_elem_for_mouse_pos;
	view->_fill_background_cb = (gui_draw_cb_t)_gui_scroll_view_fill_background;
	// TODO(PT): Might need to set a new type here
}

void gui_scroll_view_add_subview(gui_view_t* superview, gui_scroll_view_t* subview) {
	subview->window = superview->window;
	subview->superview = superview;
	subview->frame = subview->sizer_cb((gui_elem_t*)subview, subview->window->size);
	subview->content_layer_frame = rect_make(point_zero(), subview->frame.size);
	subview->parent_layer = superview->content_layer;

	printf("%s Initial view frame (subview)\n", rect_print(subview->frame));
	// Set the title inset now that we have a frame
	gui_view_set_title(subview, NULL);

	array_insert(superview->subviews, subview);
}

void gui_scroll_view_add_to_window(gui_scroll_view_t* view, gui_window_t* window) {
	view->window = window;
	view->frame = view->sizer_cb((gui_elem_t*)view, window->size);
	view->content_layer_frame = rect_make(point_zero(), view->frame.size);

	// TODO(PT): Need to free this
	// Or mabye we should replace all layers this way
	gui_layer_t* wrapper = calloc(1, sizeof(gui_layer_t));
	wrapper->fixed_layer.type = GUI_FIXED_LAYER;
	wrapper->fixed_layer.inner = window->layer;
	view->parent_layer = wrapper;

	printf("%s Initial view frame (root view)\n", rect_print(view->frame));
	// Set the title inset now that we have a frame
	gui_view_set_title(view, NULL);

	array_insert(window->views, view);
	array_insert(window->all_gui_elems, view);
}

gui_scroll_view_t* gui_scroll_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_scroll_view_t* view = gui_scroll_view_alloc();
	gui_scroll_view_init(view, window, sizer_cb);
	gui_scroll_view_add_to_window(view, window);
	return view;
}

void gui_scroll_view_destroy(gui_scroll_view_t* view) {
	ca_scrolling_layer_teardown(view->content_layer);
	array_destroy(view->subviews);
	free(view);
}
