#include "gui_text_view.h"
#include "gui_scrollbar.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _text_view_handle_mouse_entered(text_view_t* tv) {
	if (tv->mouse_entered_cb) {
		tv->mouse_entered_cb((gui_elem_t*)tv);
	}
}

static void _text_view_handle_mouse_exited(text_view_t* tv) {
	if (tv->mouse_exited_cb) {
		tv->mouse_exited_cb((gui_elem_t*)tv);
	}
}

static void _text_view_handle_mouse_moved(text_view_t* tv, Point mouse_pos) {
	if (tv->mouse_moved_cb) {
		tv->mouse_moved_cb((gui_elem_t*)tv, mouse_pos);
	}
}

static void _hide_scrollbar(text_view_t* tv) {
	tv->scrollbar->hidden = true;
	tv->right_scrollbar_inset_origin_x = 0;
	tv->right_scrollbar_inset_width = 0;
}

static void _unhide_scrollbar(text_view_t* tv) {
	tv->scrollbar->hidden = false;
	tv->right_scrollbar_inset_origin_x = rect_min_x(tv->scrollbar->frame);
	tv->right_scrollbar_inset_width = rect_max_x(tv->frame) - tv->right_scrollbar_inset_origin_x;
}

static void _update_scrollbar(text_view_t* tv) {
	uint32_t bottom_y = tv->text_box->max_text_y;
	if (bottom_y < tv->text_box->size.height) {
		_hide_scrollbar(tv);
	}
	else {
			_unhide_scrollbar(tv);
	}
}

static void _update_layout(text_view_t* tv) {
	_update_scrollbar(tv);

	Size new_text_box_size = size_make(
		tv->frame.size.width - (tv->text_box_margin * 2) - tv->right_scrollbar_inset_width,
		tv->frame.size.height - (tv->text_box_margin * 2)
	);
	// Do we need to update the size of the text box?
	if (new_text_box_size.width != tv->text_box->size.width || new_text_box_size.height != tv->text_box->size.height) {
		text_box_resize(tv->text_box, new_text_box_size);
	}
}


static void _text_view_handle_mouse_scrolled(text_view_t* tv, int8_t delta_z) {
	// If we've decided we can't scroll now, don't try to
	if (tv->scrollbar->hidden) {
		return;
	}

	uint32_t scroll_off = tv->text_box->scroll_layer->scroll_offset.height;
	uint32_t bottom_y = tv->text_box->max_text_y;
	tv->scrollbar->scroll_percent = max(0.0, min(1.0, scroll_off / (float)bottom_y));

	bool scroll_up = delta_z > 0;
	for (uint32_t i = 0; i < abs(delta_z); i++) {
		if (scroll_up) {
			// Can we scroll up any further?
			uint32_t max_visible_height = tv->text_box->size.height + tv->text_box->scroll_layer->scroll_offset.height;
			if (max_visible_height >= tv->text_box->max_text_y + tv->text_box->size.height) {
				// Can't scroll any further
				return;
			}
			text_box_scroll_up(tv->text_box);
		}
		else {
			text_box_scroll_down(tv->text_box);
		}
	}

	// Now that we've adjusted the scroll position, update the scrollbar 
	//_update_scrollbar(tv);
	tv->_priv_needs_display = true;
	tv->scrollbar->_priv_needs_display = true;
}

static void _text_view_draw(text_view_t* tv, bool is_active) {
	// Update whether the scroll bar should be visible or not
	// This happens here because we don't know when new text might've been drawn
	// TODO(PT): This can be moved if text_view absorbs text layout from text_box
	_update_layout(tv);

	// Margin
	uint32_t inner_margin_size = 6;

	// Draw a left edge in which we display the scrollbar
	if (!tv->scrollbar->hidden) {
		draw_rect(
			tv->window->layer,
			rect_make(
				point_make(tv->right_scrollbar_inset_origin_x, tv->frame.origin.y),
				size_make(tv->right_scrollbar_inset_width, tv->frame.size.height)
			),
			color_light_gray(),
			THICKNESS_FILLED
		);
	}

	// Outer margin
	Rect outer_margin = rect_make(
		tv->frame.origin,
		size_make(
			tv->frame.size.width - tv->right_scrollbar_inset_width,
			tv->frame.size.height
		)
	);

	draw_rect(
		tv->window->layer, 
		outer_margin,
		color_light_gray(),
		tv->text_box_margin - inner_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	draw_rect(
		tv->window->layer, 
		tv->frame,
		outline_color,
		1
	);

	uint32_t outer_margin_before_inner = tv->text_box_margin - inner_margin_size;
	Rect inner_margin = rect_make(
		point_make(
			rect_min_x(outer_margin) + tv->text_box_margin - inner_margin_size,
			rect_min_y(outer_margin) + tv->text_box_margin - inner_margin_size
		),
		size_make(
			outer_margin.size.width - (outer_margin_before_inner * 2),
			outer_margin.size.height - (outer_margin_before_inner * 2)
		)
	);
	draw_rect(
		tv->window->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	/*
	int inset_adjustment_x = 3;
	// Top left corner
	draw_line(
		tv->window->layer,
		line_make(
			point_make(
				inner_margin.origin.x + inset_adjustment_x,
				inner_margin.origin.y
			),
			point_make(
				inner_margin.origin.x + inner_margin_size,
				inner_margin.origin.y + inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	// Bottom left corner
	draw_line(
		tv->window->layer,
		line_make(
			point_make(
				inner_margin.origin.x + inset_adjustment_x,
				rect_max_y(inner_margin)
			),
			point_make(
				inner_margin.origin.x + inner_margin_size + inset_adjustment_x,
				rect_max_y(inner_margin) - inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);

	// Top left corner
	draw_line(
		tv->window->layer,
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

	// Bottom left corner
	draw_line(
		tv->window->layer,
		line_make(
			point_make(
				rect_max_x(inner_margin) - inset_adjustment_x,
				rect_max_y(inner_margin)
			),
			point_make(
				rect_max_x(inner_margin) - inner_margin_size - inset_adjustment_x,
				rect_max_y(inner_margin) - inner_margin_size
			)
		),
		inset_color,
		inner_margin_size
	);
	*/

	// Draw the inner text box
	Rect text_box_frame = rect_make(
		point_make(
			inner_margin.origin.x + inner_margin_size,
			inner_margin.origin.y + inner_margin_size
		),
		tv->text_box->size
	);
	draw_diagonal_insets(
		tv->window->layer,
		inner_margin,
		text_box_frame,
		inset_color,
		inner_margin_size
	);
	text_box_blit(tv->text_box, tv->window->layer, text_box_frame);
}

static void _text_view_window_resized(text_view_t* tv, Size new_window_size) {
	Rect new_frame = tv->sizer_cb((gui_elem_t*)tv, new_window_size);
	tv->frame = new_frame;
	_update_layout(tv);
	tv->_priv_needs_display = true;
}

static Rect _text_view_scrollbar_sizer(gui_scrollbar_t* sb, Size window_size) {
	// Parent is guaranteed to be a text view
	text_view_t* parent = (text_view_t*)sb->parent;
	Size parent_size = parent->frame.size;
	float scrollbar_x_margin = parent->text_box_margin * 0.125;
	float scrollbar_y_margin = 0;

	int scrollbar_width = 24;
	int scrollbar_height = parent_size.height - (parent->text_box_margin)/* - (scrollbar_y_margin * 2)*/;

	// TODO(PT): Elements should receive their parent's size instead of the window size
	Point local_origin = point_make(
		parent_size.width - scrollbar_width - parent->text_box_margin/2,
		(parent_size.height / 2) - (scrollbar_height / 2)
	);
	return rect_make(
		point_make(
			local_origin.x + parent->frame.origin.x,
			local_origin.y + parent->frame.origin.y
		),
		size_make(scrollbar_width, scrollbar_height)
	);
}

static void _text_view_scroll_pos_updated(gui_scrollbar_t* sb, float new_scroll_percent) {
	// Parent is guaranteed to be a text view
	text_view_t* parent = (text_view_t*)sb->parent;
	uint32_t bottom_y = parent->text_box->max_text_y;
	uint32_t scroll_off = bottom_y * new_scroll_percent;
	parent->text_box->scroll_layer->scroll_offset.height = scroll_off;
}

text_view_t* gui_text_view_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb) {
	text_view_t* text_view = calloc(1, sizeof(text_view_t));
	text_view->window = window;
    text_view->type = GUI_TYPE_TEXT_VIEW;

	text_view->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_text_view_handle_mouse_entered;
	text_view->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_text_view_handle_mouse_exited;
	text_view->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_text_view_handle_mouse_moved;
	text_view->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_noop;
	text_view->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_noop;
	text_view->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_noop;
	text_view->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_text_view_handle_mouse_scrolled;
	text_view->_priv_key_down_cb = (gui_key_down_cb_t)_noop;
	text_view->_priv_draw_cb = (gui_draw_cb_t)_text_view_draw;
	text_view->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_text_view_window_resized;
	text_view->_priv_needs_display = true;
	text_view->sizer_cb = sizer_cb;

	array_insert(window->text_views, text_view);
	array_insert(window->all_gui_elems, text_view);

	text_view->frame = sizer_cb((gui_elem_t*)text_view, window->size);

	text_view->text_box_margin = 12;
	text_box_t* text_box = text_box_create(
		size_make(
			frame.size.width - (text_view->text_box_margin * 2),
			frame.size.height - (text_view->text_box_margin * 2)
		),
		background_color
	);
	text_view->text_box = text_box;
	text_view->text_box->preserves_history = true;
	text_view->text_box->cache_drawing = true;

	// Must be called after text_view is added to all_gui_elems to set up the Z-order correctly
	text_view->scrollbar = gui_scrollbar_create(
		window,
		(gui_elem_t*)text_view,
		rect_zero(),
		(gui_window_resized_cb_t)_text_view_scrollbar_sizer
	);
	text_view->scrollbar->scroll_position_updated_cb = _text_view_scroll_pos_updated;
	_hide_scrollbar(text_view);

	return text_view;
}

void gui_text_view_clear(text_view_t* text_view) {
	text_box_clear(text_view->text_box);
	text_view->_priv_needs_display = true;
	text_view->scrollbar->_priv_needs_display = true;
}

void gui_text_view_clear_and_erase_history(text_view_t* text_view) {
	text_box_clear_and_erase_history(text_view->text_box);
	text_view->_priv_needs_display = true;
	text_view->scrollbar->_priv_needs_display = true;
}

void gui_text_view_puts(text_view_t* text_view, const char* str, Color color) {
	text_box_puts(text_view->text_box, str, color);
	// Only redraw if the text is visible
	uint32_t scroll_off = text_view->text_box->scroll_layer->scroll_offset.height + text_view->text_box->size.height;
	uint32_t bottom_y = text_view->text_box->max_text_y;
	if (bottom_y <= scroll_off) {
		text_view->_priv_needs_display = true;
	}

	text_view->scrollbar->_priv_needs_display = true;
}

void gui_text_view_destroy(text_view_t* text_view) {
	text_box_destroy(text_view->text_box);
	free(text_view);
}
