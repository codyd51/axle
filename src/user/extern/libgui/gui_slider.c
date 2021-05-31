#include <agx/lib/shapes.h>

#include "gui_slider.h"
#include "gui_view.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _set_needs_display(gui_slider_t* s) {
	s->_priv_needs_display = true;
	s->superview->_priv_needs_display = true;
}

static void _gui_slider_handle_mouse_entered(gui_slider_t* s) {
	_set_needs_display(s);
}

static void _gui_slider_handle_mouse_exited(gui_slider_t* s) {
	s->in_left_click = false;
	_set_needs_display(s);
}

static void _gui_slider_handle_mouse_moved(gui_slider_t* s, Point mouse_pos) {
	mouse_pos = point_make(mouse_pos.x - s->superview->frame.origin.x, mouse_pos.y - s->superview->frame.origin.y);
	if (s->slider_percent_updated_cb && s->in_left_click) {
		// TODO(PT): Need to adjust based on parent view coordinate system
		float percent = (mouse_pos.x - s->slider_origin_x) / (float)s->slidable_width;
		percent = max(min(percent, 1.0), 0.0);
		s->slider_percent = percent;
		s->slider_percent_updated_cb(s, percent);
		_set_needs_display(s);
	}
}

static void _gui_slider_handle_mouse_dragged(gui_slider_t* s, Point mouse_pos) {
	mouse_pos = point_make(mouse_pos.x - s->superview->frame.origin.x, mouse_pos.y - s->superview->frame.origin.y);
	if (s->slider_percent_updated_cb && s->in_left_click) {
		float percent = 0.0;
		if (mouse_pos.x >= (int32_t)s->slider_origin_x) {
			percent = (max(mouse_pos.x, s->slider_origin_x) - s->slider_origin_x) / (float)s->slidable_width;
		}
		percent = max(min(percent, 1.0), 0.0);

		s->slider_percent = percent;
		s->slider_percent_updated_cb(s, percent);
		_set_needs_display(s);
	}
}

static void _gui_slider_handle_mouse_left_click(gui_slider_t* s, Point mouse_pos) {
	s->in_left_click = true;

	mouse_pos = point_make(mouse_pos.x - s->superview->frame.origin.x, mouse_pos.y - s->superview->frame.origin.y);
	if (s->slider_percent_updated_cb) {
		float percent = (mouse_pos.x - s->slider_origin_x) / (float)s->slidable_width;
		percent = max(min(percent, 1.0), 0.0);
		s->slider_percent = percent;
		s->slider_percent_updated_cb(s, percent);
	}
	_set_needs_display(s);
}

static void _gui_slider_handle_mouse_left_click_ended(gui_slider_t* s, Point mouse_pos) {
	s->in_left_click = false;
	_set_needs_display(s);
}

static void _gui_slider_draw(gui_slider_t* s, bool is_active) {
	Rect r = s->frame;
	uint32_t bar_height = r.size.height * 0.75;
	uint32_t bar_margin = bar_height / 4.0;
	bar_height = max(bar_height, 1);
	bar_margin = max(bar_margin, 1);

	Rect bar_frame = rect_make(
		point_make(
			r.origin.x,
			r.origin.y + (r.size.height / 2) - (bar_height / 2)
		),
		size_make(
			r.size.width,
			bar_height
		)
	);
	gui_layer_draw_rect(
		s->superview->content_layer,
		bar_frame,
		color_dark_gray(),
		bar_margin
	);

	Rect inner_bar_frame = rect_make(
		point_make(
			rect_min_x(bar_frame) + bar_margin,
			rect_min_y(bar_frame) + bar_margin
		),
		size_make(
			bar_frame.size.width - (bar_margin * 2),
			bar_frame.size.height - (bar_margin * 2)
		)
	);
	gui_layer_draw_rect(
		s->superview->content_layer,
		inner_bar_frame,
		color_light_gray(),
		THICKNESS_FILLED
	);

	draw_diagonal_insets(
		s->superview->content_layer, 
		bar_frame, 
		inner_bar_frame, 
		color_make(120, 120, 120), 
		2
	);

	// Draw the indicator
	int indicator_width = r.size.width / 10;
	s->slidable_width = (r.size.width - (indicator_width * 2.0));
	int indicator_mid_x = s->slidable_width * s->slider_percent;
	s->slider_origin_x = rect_min_x(r) + ((r.size.width - s->slidable_width) / 2);

	uint32_t indicator_margin = r.size.height / 6;

	Rect indicator_frame = rect_make(
		point_make(
			s->slider_origin_x + indicator_mid_x - (indicator_width / 2),
			r.origin.y
		),
		size_make(
			indicator_width, 
			r.size.height
		)
	);

	gui_layer_draw_rect(
		s->superview->content_layer,
		indicator_frame,
		color_make(60, 60, 60),
		indicator_margin
	);

	if (is_active) {
		gui_layer_draw_rect(
			s->superview->content_layer,
			indicator_frame,
			color_make(200, 200, 200),
			1
		);
	}

	Rect inner_indicator = rect_make(
		point_make(
			rect_min_x(indicator_frame) + indicator_margin,
			rect_min_y(indicator_frame) + indicator_margin
		),
		size_make(
			indicator_frame.size.width - (indicator_margin * 2),
			indicator_frame.size.height - (indicator_margin * 2)
		)
	);
	gui_layer_draw_rect(
		s->superview->content_layer,
		inner_indicator,
		color_make(100, 100, 100),
		THICKNESS_FILLED
	);

	draw_diagonal_insets(
		s->superview->content_layer, 
		indicator_frame, 
		inner_indicator, 
		color_make(120, 120, 120),
		2
	);
}

static void _slider_window_resized(gui_slider_t* s, Size new_window_size) {
	Rect new_frame = s->sizer_cb((gui_elem_t*)s, new_window_size);
	s->frame = new_frame;
	s->_priv_needs_display = true;
}

gui_slider_t* gui_slider_create(gui_view_t* superview, gui_window_resized_cb_t sizer_cb) {
	gui_slider_t* slider = calloc(1, sizeof(gui_slider_t));
	slider->window = superview->window;
	slider->superview = superview;
	slider->type = GUI_TYPE_SLIDER;
	slider->border_margin = 4;

	slider->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_gui_slider_handle_mouse_entered;
	slider->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_gui_slider_handle_mouse_exited;
	slider->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_gui_slider_handle_mouse_moved;
	slider->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_gui_slider_handle_mouse_dragged;
	slider->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_gui_slider_handle_mouse_left_click;
	slider->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_gui_slider_handle_mouse_left_click_ended;
	slider->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_noop;
	slider->_priv_key_down_cb = (gui_key_down_cb_t)_noop;
	slider->_priv_key_up_cb = (gui_key_up_cb_t)_noop;
	slider->_priv_draw_cb = (gui_draw_cb_t)_gui_slider_draw;
	slider->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_slider_window_resized;
	slider->_priv_needs_display = true;
	slider->sizer_cb = sizer_cb;

	array_insert(superview->subviews, slider);

	slider->frame = sizer_cb((gui_elem_t*)slider, slider->window->size);

	return slider;
}

void gui_slider_destroy(gui_slider_t* slider) {
	// TODO(PT): Does the slider need to be removed from superview->subviews?
	free(slider);
}
