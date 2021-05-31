#include "gui_scrollbar.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _set_needs_display(gui_scrollbar_t* sb) {
	sb->_priv_needs_display = true;
	sb->parent->base._priv_needs_display = true;
}

static void _gui_scrollbar_handle_mouse_entered(gui_scrollbar_t* sb) {
	_set_needs_display(sb);
}

static void _gui_scrollbar_handle_mouse_exited(gui_scrollbar_t* sb) {
	sb->in_left_click = false;
	_set_needs_display(sb);
}

static void _gui_scrollbar_handle_mouse_moved(gui_scrollbar_t* sb, Point mouse_pos) {
	if (sb->scroll_position_updated_cb && sb->in_left_click) {
		// TODO(PT): Save inner_frame on the scrollbar for calc here
		float percent = (mouse_pos.y - sb->frame.origin.y) / (float)sb->frame.size.height;
		percent = max(min(percent, 1.0), 0.0);
		sb->scroll_percent = percent;
		sb->scroll_position_updated_cb(sb, percent);
		_set_needs_display(sb);
	}
}

static void _gui_scrollbar_handle_mouse_dragged(gui_scrollbar_t* sb, Point mouse_pos) {
	if (sb->scroll_position_updated_cb && sb->in_left_click) {
		// TODO(PT): Save inner_frame on the scrollbar for calc here
		float percent = (mouse_pos.y - sb->frame.origin.y) / (float)sb->frame.size.height;
		percent = max(min(percent, 1.0), 0.0);
		sb->scroll_percent = percent;
		sb->scroll_position_updated_cb(sb, percent);
		_set_needs_display(sb);
	}
}

static void _gui_scrollbar_handle_mouse_left_click(gui_scrollbar_t* sb, Point mouse_pos) {
	sb->in_left_click = true;

	if (sb->scroll_position_updated_cb) {
		// TODO(PT): Save inner_frame on the scrollbar for calc here
		float percent = (mouse_pos.y - sb->frame.origin.y) / (float)sb->frame.size.height;
		percent = max(min(percent, 1.0), 0.0);
		sb->scroll_percent = percent;
		sb->scroll_position_updated_cb(sb, percent);
		_set_needs_display(sb);
	}
}

static void _gui_scrollbar_handle_mouse_left_click_ended(gui_scrollbar_t* sb, Point mouse_pos) {
	sb->in_left_click = false;
	_set_needs_display(sb);
}

static void _gui_scrollbar_window_resized(gui_scrollbar_t* sb, Size new_window_size) {
	Rect new_frame = sb->sizer_cb((gui_elem_t*)sb, new_window_size);
	sb->frame = new_frame;
	_set_needs_display(sb);
}

static void _gui_scrollbar_draw(gui_scrollbar_t* sb, bool is_active) {
	// TODO(PT): Move "hidden" to shared fields
	if (sb->hidden) {
		return;
	}

	// Margin
	uint32_t margin_size = 10;
	uint32_t inner_margin_size = 4;

	// Fudge the frame so that we overlap with the border of the main content view we're attached to
	// This way, we don't have a 'double border' of the main content's right edge and the
	// scrollbar's left edge
	// Note this adjustment should exactly match the border margin set up by the parent
	// TODO(PT): Compute from the parent
	Rect sb_frame = sb->frame;
	sb_frame.origin.x -= 6;
	sb_frame.size.width += 6;
	Rect local_frame = rect_make(point_zero(), sb_frame.size);

	// Outer margin
	Rect outer_margin = local_frame;
	gui_layer_draw_rect(
		sb->layer,
		local_frame,
		color_light_gray(),
		margin_size - inner_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	/*
	draw_rect(
		sb->layer,
		local_frame,
		outline_color,
		1
	);
	*/

	uint32_t outer_margin_before_inner = margin_size - inner_margin_size;
	Rect inner_margin = rect_make(
		point_make(
			rect_min_x(outer_margin) + margin_size - inner_margin_size,
			rect_min_y(outer_margin) + margin_size - inner_margin_size
		),
		size_make(
			outer_margin.size.width - (outer_margin_before_inner * 2),
			outer_margin.size.height - (outer_margin_before_inner * 2)
		)
	);
	gui_layer_draw_rect(
		sb->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Rect inner_frame = rect_make(
		point_make(
			inner_margin.origin.x + inner_margin_size,
			inner_margin.origin.y + inner_margin_size
		),
		size_make(
			inner_margin.size.width - (inner_margin_size * 2),
			inner_margin.size.height - (inner_margin_size * 2)
		)
	);
	draw_diagonal_insets(
		sb->layer,
		inner_margin,
		inner_frame,
		color_make(50, 50, 50),
		2
	);

	gui_layer_draw_rect(
		sb->layer,
		inner_frame,
		color_black(),
		THICKNESS_FILLED
	);

	// Draw the indicator
	float indicator_mid_y = sb->scroll_percent * inner_frame.size.height;
	float indicator_height = inner_frame.size.height / 6;
	float indicator_origin_y = inner_frame.origin.y + indicator_mid_y - (indicator_height / 2);
	int indicator_x_margin = inner_frame.size.width * 0.05;
	int indicator_y_margin = inner_frame.size.width * 0.15;
	int indicator_width = (inner_frame.size.width * 0.8) - (indicator_x_margin * 2);

	if (indicator_origin_y <= inner_frame.origin.y + indicator_y_margin) {
		int overhang = abs(indicator_origin_y);
		indicator_height -= overhang;
		indicator_origin_y = inner_frame.origin.y + indicator_y_margin;
	}
	else if (indicator_origin_y + indicator_height + indicator_y_margin >= rect_max_y(inner_frame)) {
		int overhang = indicator_origin_y + indicator_height - rect_max_y(inner_frame);
		indicator_height -= overhang + indicator_y_margin;
	}

	Rect indicator_frame = rect_make(
		point_make(
			inner_frame.origin.x + (inner_frame.size.width / 2) - (indicator_width / 2),
			indicator_origin_y
		),
		size_make(indicator_width, indicator_height)
	);
	gui_layer_draw_rect(
		sb->layer,
		indicator_frame,
		color_make(100, 100, 100),
		THICKNESS_FILLED
	);
	Color indicator_outline_color = is_active ? color_make(200, 200, 200) : color_make(60, 60, 60);
	gui_layer_draw_rect(
		sb->layer,
		indicator_frame,
		outline_color,
		1
	);

	gui_layer_blit_layer(
		sb->window->layer, 
		sb->layer,
		sb_frame,
		local_frame
	);
}

gui_scrollbar_t* gui_scrollbar_create(gui_window_t* window, gui_elem_t* parent, Rect frame, gui_window_resized_cb_t sizer_cb) {
	gui_scrollbar_t* sb = calloc(1, sizeof(gui_scrollbar_t));
	sb->parent = parent;
	sb->window = window;
	sb->frame = frame;
    sb->type = GUI_TYPE_SCROLLBAR;

	sb->layer = gui_layer_create(
		GUI_FIXED_LAYER, 
		size_make(
			// Max possible width of a scrollbar
			50,
			// Max possible height of a scrollbar
            _gui_screen_resolution().height
		)
	);

	sb->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_gui_scrollbar_handle_mouse_entered;
	sb->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_gui_scrollbar_handle_mouse_exited;
	sb->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_gui_scrollbar_handle_mouse_moved;
	sb->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_gui_scrollbar_handle_mouse_dragged;
	sb->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_gui_scrollbar_handle_mouse_left_click;
	sb->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_gui_scrollbar_handle_mouse_left_click_ended;
	sb->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_noop;
	sb->_priv_key_down_cb = (gui_key_down_cb_t)_noop;
	sb->_priv_key_up_cb = (gui_key_up_cb_t)_noop;
	sb->_priv_draw_cb = (gui_draw_cb_t)_gui_scrollbar_draw;
	sb->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_gui_scrollbar_window_resized;
	sb->_priv_needs_display = true;
	sb->sizer_cb = sizer_cb;

	sb->frame = sizer_cb((gui_elem_t*)sb, window->size);

	//array_insert(window->all_gui_elems, sb);

	return sb;
}
