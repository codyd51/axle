#include <stdio.h>
#include <string.h>

#include <stdlibadd/assert.h>

#include <agx/lib/shapes.h>
#include <agx/font/font.h>

#include "gui_scroll_view.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _hide_scrollbar(gui_scroll_view_t* sv) {
	sv->scrollbar->hidden = true;
	//sv->right_scrollbar_inset_origin_x = 0;
	//sv->right_scrollbar_inset_width = 0;
}

static void _unhide_scrollbar(gui_scroll_view_t* sv) {
	sv->scrollbar->hidden = false;
	//sv->right_scrollbar_inset_origin_x = rect_min_x(sv->scrollbar->frame);
	//sv->right_scrollbar_inset_width = rect_max_x(sv->frame) - sv->right_scrollbar_inset_origin_x;
}

static void _update_scrollbar(gui_scroll_view_t* sv) {
	gui_view_t* base = &(sv->base);
	uint32_t bottom_y = base->content_layer->scroll_layer.max_y;
	if (bottom_y < base->content_layer->scroll_layer.scroll_offset.y) {
	//	_hide_scrollbar(sv);
	}
	else {
		_unhide_scrollbar(sv);
	}
	// testing
	_unhide_scrollbar(sv);
}

static void _update_layout(gui_scroll_view_t* sv) {
	_update_scrollbar(sv);
}

void gui_scroll_view_alloc_dynamic_fields(gui_scroll_view_t* scroll_view) {
	Size screen_res = _gui_screen_resolution();
	gui_view_t* base = &(scroll_view->base);
	base->content_layer = gui_layer_create(
		GUI_SCROLL_LAYER, 
		size_make(
			screen_res.width,
			screen_res.height * 4
		)
	);
	base->subviews = array_create(128);
}

gui_scroll_view_t* gui_scroll_view_alloc(void) {
	gui_scroll_view_t* v = calloc(1, sizeof(gui_scroll_view_t));
	gui_scroll_view_alloc_dynamic_fields(v);
	return v;
}

static void _handle_mouse_scrolled(gui_scroll_view_t* sv, int8_t delta_z) {
	gui_view_t* base = &(sv->base);
	base->content_layer->scroll_layer.scroll_offset.y += delta_z * 40;

	int32_t scroll_off = base->content_layer->scroll_layer.scroll_offset.y;
	int32_t bottom_y = base->content_layer->scroll_layer.max_y;
	base->content_layer->scroll_layer.scroll_offset.y = max(0, min(bottom_y, scroll_off));

	// If we've decided we can't scroll now, don't try to
	/*
	if (tv->scrollbar->hidden) {
		return;
	}
	*/

	sv->scrollbar->scroll_percent = max(0.0, min(1.0, scroll_off / (float)bottom_y));
	_update_layout(sv);
}

gui_elem_t* _gui_scroll_view_elem_for_mouse_pos(gui_scroll_view_t* sv, Point mouse_pos) {
	if (rect_contains_point(sv->scrollbar->frame, mouse_pos)) {
		return (gui_elem_t*)sv->scrollbar;
	}

	gui_view_t* base = &(sv->base);
	mouse_pos.x += base->content_layer->scroll_layer.scroll_offset.x;
	mouse_pos.y += base->content_layer->scroll_layer.scroll_offset.y;
	return gui_view_elem_for_mouse_pos((gui_view_t*)sv, mouse_pos);
}

static void _gui_scroll_view_fill_background(gui_scroll_view_t* sv, bool is_active) {
	gui_view_t* base = &(sv->base);
	gui_layer_draw_rect(
		base->content_layer, 
		rect_make(
			point_make(
				base->content_layer->scroll_layer.scroll_offset.x,
				base->content_layer->scroll_layer.scroll_offset.y
			),
			base->content_layer_frame.size
		), 
		base->background_color, 
		THICKNESS_FILLED
	);
}

static Rect _scrollbar_sizer(gui_scrollbar_t* sb, Size window_size) {
	gui_scroll_view_t* parent = (gui_scroll_view_t*)sb->parent;
	Rect frame = parent->base.frame;
	if (frame.size.width == 0 && frame.size.height == 0) {
		return rect_zero();
	}

	int scrollbar_width = 30;
	int scrollbar_height = frame.size.height;

	Point origin = point_make(
		rect_max_x(frame) - scrollbar_width,
		rect_min_y(frame)
	);
	
	return rect_make(
		origin,
		size_make(
			scrollbar_width, 
			scrollbar_height
		)
	);
}

static void _scroll_view_window_resized(gui_scroll_view_t* sv, Size new_window_size) {
	_gui_view_resize(sv, new_window_size);

	sv->scrollbar->frame = sv->scrollbar->sizer_cb((gui_elem_t*)sv->scrollbar, new_window_size);
	// The content view will be its original size
	// Subtract the scrollbar width
	// Add in the right border, since that's now managed by the scrollbar
	Size new_content_view_size = size_make(
		sv->base.content_layer_frame.size.width - sv->scrollbar->frame.size.width,
		sv->base.content_layer_frame.size.height
	);
	sv->base.content_layer_frame.size = new_content_view_size;

	_gui_view_resize_invoke_callbacks(sv, new_window_size);
}

void _gui_scroll_view_draw(gui_scroll_view_t* sv, bool is_active) {
	// Draw a left edge in which we display the scrollbar
	Rect frame_excluding_scrollbar = rect_make(
		sv->base.frame.origin,
		size_make(
			sv->base.frame.size.width - sv->scrollbar->frame.size.width,
			sv->base.frame.size.height
		)
	);
	if (frame_excluding_scrollbar.size.width <= 0 || frame_excluding_scrollbar.size.height <= 0) {
		return;
	}
	_gui_view_draw_main_content_in_rect((gui_view_t*)sv, is_active, frame_excluding_scrollbar);

	if (!sv->scrollbar->hidden || true) {
		bool is_active = sv->base.window->hover_elem == sv->scrollbar;
		sv->scrollbar->_priv_draw_cb((gui_elem_t*)sv->scrollbar, is_active);
	}

	_gui_view_draw_active_indicator((gui_view_t*)sv, is_active);
}

static void _scrollbar_position_updated(gui_scrollbar_t* sb, float new_scroll_percent) {
	// Parent is guaranteed to be a scroll view
	gui_scroll_view_t* parent = (gui_scroll_view_t*)sb->parent;
	uint32_t bottom_y = parent->base.content_layer->scroll_layer.max_y;
	uint32_t scroll_off = bottom_y * new_scroll_percent;
	parent->base.content_layer->scroll_layer.scroll_offset.y = scroll_off;
}

void gui_scroll_view_init(gui_scroll_view_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_view_init((gui_view_t*)view, window, sizer_cb);

	gui_view_t* base = &(view->base);
	base->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_handle_mouse_scrolled;
    base->elem_for_mouse_pos_cb = (gui_view_elem_for_mouse_pos_cb_t)_gui_scroll_view_elem_for_mouse_pos;
	base->_fill_background_cb = (gui_draw_cb_t)_gui_scroll_view_fill_background;
	base->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_scroll_view_window_resized;
	base->_priv_draw_cb = (gui_draw_cb_t)_gui_scroll_view_draw;
	// TODO(PT): Might need to set a new type here

	// Must be called after text_view is added to all_gui_elems to set up the Z-order correctly
	view->scrollbar = gui_scrollbar_create(
		window,
		(gui_elem_t*)view,
		rect_zero(),
		(gui_window_resized_cb_t)_scrollbar_sizer
	);
	view->scrollbar->scroll_position_updated_cb = (gui_scrollbar_updated_cb_t)_scrollbar_position_updated;
}

void gui_scroll_view_add_subview(gui_view_t* superview, gui_scroll_view_t* subview) {
	gui_view_t* base = &(subview->base);
	base->window = superview->window;
	base->superview = superview;
	base->frame = base->sizer_cb((gui_elem_t*)subview, base->window->size);
	base->content_layer_frame = rect_make(point_zero(), base->frame.size);
	base->parent_layer = superview->content_layer;

	// Fill the background color and set the title inset now that we have a frame
	base->_fill_background_cb(subview, false);
	gui_view_set_title(base, NULL);

	array_insert(superview->subviews, subview);
	_unhide_scrollbar(subview);
}

void gui_scroll_view_add_to_window(gui_scroll_view_t* view, gui_window_t* window) {
	gui_view_t* view_base = &(view->base);
	view_base->window = window;
	view_base->frame = view_base->sizer_cb((gui_elem_t*)view, window->size);
	view_base->content_layer_frame = rect_make(point_zero(), view_base->frame.size);
	view_base->parent_layer = window->layer;

	// Set the title inset now that we have a frame
	// Fill the background color and set the title inset now that we have a frame
	view_base->_fill_background_cb(view_base, false);
	gui_view_set_title((gui_view_t*)view, NULL);

	array_insert(window->views, view);
	array_insert(window->all_gui_elems, view);
	_unhide_scrollbar(view);
}

gui_scroll_view_t* gui_scroll_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_scroll_view_t* view = gui_scroll_view_alloc();
	gui_scroll_view_init(view, window, sizer_cb);
	gui_scroll_view_add_to_window(view, window);
	return view;
}

void gui_scroll_view_destroy(gui_scroll_view_t* view) {
	gui_layer_teardown(view->base.content_layer);
	array_destroy(view->base.subviews);
	free(view);
}
