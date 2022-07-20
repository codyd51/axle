#include <string.h>
#include <ctype.h>

#include "gui_text_input.h"
#include "gui_scrollbar.h"
#include "libgui.h"
#include "utils.h"

gui_text_input_t* gui_text_input_alloc(void) {
	gui_text_input_t* v = calloc(1, sizeof(gui_text_input_t));
	gui_text_view_alloc_dynamic_fields((gui_text_view_t*)v);
	return v;
}

static bool _is_key_shift(uint32_t ch) {
	return ch == KEY_IDENT_LEFT_SHIFT || ch == KEY_IDENT_RIGHT_SHIFT;
}

static void _gui_text_input_handle_key_down(gui_text_input_t* view, uint32_t ch) {
	if (_is_key_shift(ch)) {
		view->_is_shift_held = true;
		return;
	}

	if (view->_is_shift_held) {
		ch = toupper(ch);
	}

	char str[4] = {ch};
	gui_text_view_puts((gui_text_view_t*)view, (const char*)&str, view->font_color);

	if (view->key_down_cb) {
		view->key_down_cb((gui_elem_t*)view, ch);
	}
}

static void _gui_text_input_handle_key_up(gui_text_input_t* view, uint32_t ch) {
	if (_is_key_shift(ch)) {
		view->_is_shift_held = false;
		return;
	}
}

static void _gui_text_input_draw(gui_text_input_t* ti, bool is_active) {
	_gui_scroll_view_draw((gui_scroll_view_t*)ti, is_active);

	if (ti->_input_carat_visible) {
		uint32_t input_indicator_body_width = max(2, ti->font_size.width / 4);
		Rect input_indicator = rect_make(
			point_make(
				rect_min_x(ti->content_layer_frame) + ti->cursor_pos.x + (input_indicator_body_width / 2),
				rect_min_y(ti->content_layer_frame) + ti->cursor_pos.y + 2
			),
			size_make(
				input_indicator_body_width, 
				ti->font_size.height - 4
			)
		);

		gui_layer_draw_rect(
			ti->parent_layer,
			input_indicator,
			color_gray(),
			THICKNESS_FILLED
		);
		gui_layer_draw_rect(
			ti->parent_layer,
			rect_make(
				point_make(
					rect_min_x(input_indicator) - (input_indicator_body_width / 2),
					rect_min_y(input_indicator)
				),
				size_make(
					input_indicator_body_width * 2, 
					2
				)
			),
			color_gray(),
			THICKNESS_FILLED
		);
		gui_layer_draw_rect(
			ti->parent_layer,
			rect_make(
				point_make(
					rect_min_x(input_indicator) - (input_indicator_body_width / 2),
					rect_max_y(input_indicator) - 2
				),
				size_make(
					input_indicator_body_width * 2, 
					2
				)
			),
			color_gray(),
			THICKNESS_FILLED
		);
	}
}

void gui_text_input_init(gui_text_input_t* view, gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_text_view_init((gui_text_view_t*)view, window, sizer_cb);
	view->_priv_key_down_cb = (gui_key_down_cb_t)_gui_text_input_handle_key_down;
	view->_priv_key_up_cb = (gui_key_up_cb_t)_gui_text_input_handle_key_up;
	view->_priv_draw_cb = (gui_draw_cb_t)_gui_text_input_draw;
	view->font_color = color_black();
}

static void _toggle_text_indicator(gui_text_input_t* ti) {
	ti->_input_carat_visible = !ti->_input_carat_visible;
	gui_timer_start(400, (gui_timer_cb_t)_toggle_text_indicator, ti);
}

static void _kickoff_flicker_timer(gui_text_input_t* ti) {
	gui_timer_start(400, (gui_timer_cb_t)_toggle_text_indicator, ti);
}

void gui_text_input_add_subview(gui_view_t* superview, gui_text_input_t* subview) {
	gui_text_view_add_subview(superview, (gui_text_view_t*)subview);
	_kickoff_flicker_timer(subview);
}

void gui_text_input_add_to_window(gui_text_input_t* view, gui_window_t* window) {
	gui_text_view_add_to_window((gui_text_view_t*)view, window);
	_kickoff_flicker_timer(view);
}

gui_text_input_t* gui_text_input_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_text_input_t* view = gui_text_input_alloc();
	gui_text_input_init(view, window, sizer_cb);
	gui_text_input_add_to_window(view, window);
	return view;
}

void gui_text_input_clear(gui_text_input_t* text_input) {
	gui_text_view_clear((gui_text_view_t*)text_input);
}
