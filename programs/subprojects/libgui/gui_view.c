#include <stdio.h>
#include <string.h>
#include <libagx/lib/shapes.h>
#include <libagx/font/font.h>

#include "gui_view.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _view_handle_mouse_entered(gui_view_t* v) {
	if (v->mouse_entered_cb) {
		v->mouse_entered_cb((gui_elem_t*)v);
	}
}

static void _view_handle_mouse_exited(gui_view_t* v) {
	if (v->mouse_exited_cb) {
		v->mouse_exited_cb((gui_elem_t*)v);
	}
}

static void _view_handle_mouse_moved(gui_view_t* v, Point mouse_pos) {
	if (v->mouse_moved_cb) {
		v->mouse_moved_cb((gui_elem_t*)v, mouse_pos);
	}
}

static void _view_handle_mouse_dragged(gui_view_t* v, Point mouse_pos) {
	if (v->mouse_dragged_cb) {
		v->mouse_dragged_cb((gui_elem_t*)v, mouse_pos);
	}
}

static void _view_handle_left_click(gui_view_t* v, Point mouse_pos) {
	if (v->left_click_cb) {
		v->left_click_cb((gui_elem_t*)v, mouse_pos);
	}
}

static void _view_handle_key_down(gui_view_t* v, uint32_t ch) {
	if (v->key_down_cb) {
		v->key_down_cb((gui_elem_t*)v, ch);
	}
}

static void _view_handle_key_up(gui_view_t* v, uint32_t ch) {
	if (v->key_up_cb) {
		v->key_up_cb((gui_elem_t*)v, ch);
	}
}

void gui_view_set_title(gui_view_t* v, char* title) {
	if (v->_title != NULL) {
		// Free the previous ttile
		free(v->_title);
		v->_title = NULL;
	}

	v->_title_inset = rect_make(v->frame.origin, size_zero());
	v->title_bar_height = 0;

	if (title != NULL) {
		v->_title = strdup(title);
		//v->title_bar_height = max(10, v->frame.size.height / 14);
		v->title_bar_height = 14;
		// View title
		v->_title_inset.size = size_make(
			v->frame.size.width,
			v->title_bar_height
		);
	}
	else {
		v->_title = NULL;
	}
	// Shrink the content view to account for the view title, if necessary
	v->content_layer_frame = rect_make(
		point_make(
			v->frame.origin.x + v->border_margin,
			v->frame.origin.y + v->border_margin + v->title_bar_height
		),
		size_make(
			v->frame.size.width - (v->border_margin * 2),
			v->frame.size.height - (v->border_margin * 2) - v->title_bar_height
		)
	);
}

static void _gui_view_fill_background(gui_view_t* v, bool is_active) {
	gui_layer_draw_rect(
		v->content_layer, 
		rect_make(
			point_zero(), 
			v->content_layer_frame.size
		), 
		v->background_color, 
		THICKNESS_FILLED
	);
}

void _gui_view_draw_main_content_in_rect(gui_view_t* v, bool is_active, Rect r) {
	// Margin
	uint32_t inner_margin_size = v->border_margin / 2;
	uint32_t outer_margin_size = v->border_margin - inner_margin_size;

	if (v->_title != NULL) {
		// View title
		gui_layer_draw_rect(
			v->parent_layer,
			v->_title_inset,
			color_light_gray(),
			THICKNESS_FILLED
		);

		uint32_t font_inset = max(4, v->_title_inset.size.height / 4);

		Point cursor = point_make(
			// Align the left edge with where the visual bevel begins
			rect_min_x(r) + outer_margin_size,
			rect_min_y(r) + font_inset
		);
		uint32_t font_height = min(30, v->_title_inset.size.height - font_inset);
		uint32_t font_width = max(6, font_height * 0.8);
		for (int i = 0; i < strlen(v->_title); i++) {
			gui_layer_draw_char(
				v->parent_layer,
				v->_title[i],
				cursor.x,
				cursor.y,
				color_black(),
				size_make(font_height, font_height)
			);
			cursor.x += font_width + 0;
		}
	}

	// Outer margin
	Rect outer_margin = rect_make(
		point_make(
			rect_min_x(r),
			rect_max_y(v->_title_inset)
		),
		size_make(
			r.size.width,
			r.size.height - v->_title_inset.size.height 
		)
	);
	gui_layer_draw_rect(
		v->parent_layer,
		outer_margin,
		color_light_gray(),
		outer_margin_size
	);

	uint32_t outer_margin_before_inner = v->border_margin - inner_margin_size;
	Rect inner_margin = rect_make(
		point_make(
			rect_min_x(outer_margin) + v->border_margin - inner_margin_size,
			rect_min_y(outer_margin) + v->border_margin - inner_margin_size
		),
		size_make(
			outer_margin.size.width - (outer_margin_before_inner * 2),
			outer_margin.size.height - (outer_margin_before_inner * 2)
		)
	);
	gui_layer_draw_rect(
		v->parent_layer,
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	int inset_adjustment_x = 3;
	// Top left corner
	gui_layer_draw_line(
		v->parent_layer,
		line_make(
			point_make(
				inner_margin.origin.x + inset_adjustment_x,
				inner_margin.origin.y
			),
			point_make(
				inner_margin.origin.x + inner_margin_size + inset_adjustment_x,
				inner_margin.origin.y + inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	// Bottom left corner
	gui_layer_draw_line(
		v->parent_layer,
		line_make(
			point_make(
				inner_margin.origin.x + inset_adjustment_x,
				rect_max_y(inner_margin) - 1
			),
			point_make(
				inner_margin.origin.x + inner_margin_size + inset_adjustment_x,
				rect_max_y(inner_margin) - 1 - inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	// Top right corner
	gui_layer_draw_line(
		v->parent_layer,
		line_make(
			point_make(
				rect_max_x(inner_margin) - inset_adjustment_x,
				rect_min_y(inner_margin)
			),
			point_make(
				rect_max_x(inner_margin) - inner_margin_size - inset_adjustment_x,
				rect_min_y(inner_margin) + inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	// Bottom right corner
	gui_layer_draw_line(
		v->parent_layer,
		line_make(
			point_make(
				rect_max_x(inner_margin) - inset_adjustment_x,
				rect_max_y(inner_margin) - 1
			),
			point_make(
				rect_max_x(inner_margin) - inner_margin_size - inset_adjustment_x,
				rect_max_y(inner_margin) - 1 - inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	if (!v->controls_content_layer) {
		v->_fill_background_cb((gui_elem_t*)v, is_active);
	}

	// Draw subviews
	for (uint32_t j = 0; j < v->subviews->size; j++) {
		gui_elem_t* subview = array_lookup(v->subviews, j);

		//if (subview->base._priv_needs_display) {
			is_active = v->window->hover_elem == subview;
			subview->base._priv_draw_cb(subview, is_active);
			subview->base._priv_needs_display = false;
		//}
	}

	// Draw the inner content layer
	gui_layer_blit_layer(
		v->parent_layer,
		v->content_layer,
		v->content_layer_frame,
		rect_make(
			point_zero(),
			v->content_layer_frame.size
		)
	);
}

void _gui_view_draw_active_indicator(gui_view_t* v, bool is_active) {
	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	gui_layer_draw_rect(
		v->parent_layer,
		v->frame,
		outline_color,
		1
	);
}

void _gui_view_draw(gui_view_t* v, bool is_active) {
	_gui_view_draw_main_content_in_rect(v, is_active, v->frame);
	_gui_view_draw_active_indicator(v, is_active);
}

void _gui_view_resize(gui_view_t* v, Size new_window_size) {
	Rect new_frame = v->sizer_cb((gui_elem_t*)v, new_window_size);
	v->frame = new_frame;

	v->content_layer_frame = rect_make(
		point_make(
			v->frame.origin.x + v->border_margin,
			v->frame.origin.y + v->border_margin + v->title_bar_height
		),
		size_make(
			new_frame.size.width - (v->border_margin * 2),
			new_frame.size.height - (v->border_margin * 2) - v->title_bar_height
		)
	);
	v->_title_inset.origin = v->frame.origin;
	v->_title_inset.size.width = v->frame.size.width;

	v->_priv_needs_display = true;
}

void _gui_view_resize_invoke_callbacks(gui_view_t* v, Size new_window_size) {
	if (v->window_resized_cb) {
		v->window_resized_cb((gui_elem_t*)v, new_window_size);
	}

	// Inform subviews
	for (uint32_t j = 0; j < v->subviews->size; j++) {
		gui_elem_t* sub_elem = array_lookup(v->subviews, j);
		sub_elem->base._priv_window_resized_cb(sub_elem, new_window_size);
	}
}

static void _view_window_resized(gui_view_t* v, Size new_window_size) {
	_gui_view_resize(v, new_window_size);
	_gui_view_resize_invoke_callbacks(v, new_window_size);
}

void gui_view_alloc_dynamic_fields(gui_view_t* view) {
	view->content_layer = gui_layer_create(GUI_FIXED_LAYER, _gui_screen_resolution());
	view->subviews = array_create(128);
}

gui_view_t* gui_view_alloc(void) {
	gui_view_t* v = calloc(1, sizeof(gui_view_t));
	gui_view_alloc_dynamic_fields(v);
	return v;
}

gui_elem_t* gui_view_elem_for_mouse_pos(gui_view_t* view, Point mouse_pos) {
	// Check if we should instead route to a subview
	for (uint32_t j = 0; j < view->subviews->size; j++) {
		gui_elem_t* sub_elem = array_lookup(view->subviews, j);
		Point offset = view->content_layer_frame.origin;
		Rect r = sub_elem->base.frame;
		r.origin.x += offset.x;
		r.origin.y += offset.y;
		if (rect_contains_point(r, mouse_pos)) {
			return sub_elem;
		}
	}
	// TODO(PT): We should repeat the above step to handle recursively nested views
	return (gui_elem_t*)view;
}

void gui_view_init(gui_view_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	view->type = GUI_TYPE_VIEW;
	view->border_margin = 12;

	view->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_view_handle_mouse_entered;
	view->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_view_handle_mouse_exited;
	view->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_view_handle_mouse_moved;
	view->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_view_handle_mouse_dragged;
	view->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_view_handle_left_click;
	view->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_noop;
	view->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_noop;
	view->_priv_key_down_cb = (gui_key_down_cb_t)_view_handle_key_down;
	view->_priv_key_up_cb = (gui_key_up_cb_t)_view_handle_key_up;
	view->_priv_draw_cb = (gui_draw_cb_t)_gui_view_draw;
	view->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_view_window_resized;
	view->_priv_needs_display = true;
    view->elem_for_mouse_pos_cb = (gui_view_elem_for_mouse_pos_cb_t)gui_view_elem_for_mouse_pos;
	view->sizer_cb = sizer_cb;
	view->_fill_background_cb = (gui_draw_cb_t)_gui_view_fill_background;

	view->background_color = color_black();
}

void gui_view_add_subview(gui_view_t* superview, gui_view_t* subview) {
	subview->window = superview->window;
	subview->superview = superview;
	subview->frame = subview->sizer_cb((gui_elem_t*)subview, subview->window->size);
	subview->content_layer_frame = rect_make(point_zero(), subview->frame.size);
	subview->parent_layer = superview->content_layer;

	// Set the title inset now that we have a frame
	gui_view_set_title(subview, NULL);

	array_insert(superview->subviews, subview);
}

void gui_view_add_to_window(gui_view_t* view, gui_window_t* window) {
	view->window = window;
	view->frame = view->sizer_cb((gui_elem_t*)view, window->size);
	view->content_layer_frame = rect_make(point_zero(), view->frame.size);
	view->parent_layer = window->layer;

	// Set the title inset now that we have a frame
	gui_view_set_title(view, NULL);

	array_insert(window->views, view);
	array_insert(window->all_gui_elems, view);
}

gui_view_t* gui_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_view_t* view = gui_view_alloc();
	gui_view_init(view, window, sizer_cb);
	gui_view_add_to_window(view, window);
	return view;
}

void gui_view_remove_from_superview_and_destroy(gui_view_t** view_ref) {
	// Window-based
	/*
	gui_view_t* view = *view_ref;
	gui_window_t* window = view->window;
	int window_subview_idx = array_index(window->views, view);
	array_remove(window->views, window_subview_idx);
	int all_elems_idx = array_index(window->all_gui_elems, view);
	array_remove(window->all_gui_elems, all_elems_idx);
	gui_view_destroy(view);
	// Prevent UAF
	*view_ref = NULL;
	*/

	// Superview-based
	gui_view_t* subview = *view_ref;
	gui_view_t* superview = subview->superview;
	int subview_idx = array_index(superview->subviews, subview);
	array_remove(superview->subviews, subview_idx);
	gui_view_destroy(subview);
	// Prevent UAF
	*view_ref = NULL;
}

void gui_view_destroy(gui_view_t* view) {
	if (view->teardown_cb) {
		view->teardown_cb((gui_elem_t*)view);
	}
	gui_layer_teardown(view->content_layer);
	//assert(view->subviews->size == 0, "subviews must be cleaned up");
	array_destroy(view->subviews);
	free(view->_title);
	free(view);
}
