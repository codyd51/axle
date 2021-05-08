#include <stdio.h>
#include <string.h>
#include <agx/lib/shapes.h>
#include <agx/font/font.h>

#include "gui_view.h"
#include "libgui.h"

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
	v->_title = title;
	v->_title_inset = rect_make(v->frame.origin, size_zero());
	v->title_bar_height = 0;

	if (v->_title != NULL) {
		v->title_bar_height = max(10, v->frame.size.height / 14);
		// View title
		v->_title_inset.size = size_make(
			v->frame.size.width,
			v->title_bar_height
		);
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

static void _gui_view_draw(gui_view_t* v, bool is_active) {
	// Margin
	uint32_t inner_margin_size = v->border_margin / 2;
	uint32_t outer_margin_size = v->border_margin - inner_margin_size;

	if (v->_title != NULL) {
		// View title
		draw_rect(
			v->window->layer,
			v->_title_inset,
			color_light_gray(),
			THICKNESS_FILLED
		);

		uint32_t font_inset = max(4, v->_title_inset.size.height / 4);

		Point cursor = point_make(
			// Align the left edge with where the visual bevel begins
			v->frame.origin.x + outer_margin_size,
			v->frame.origin.y + font_inset
		);
		uint32_t font_height = min(30, v->_title_inset.size.height - font_inset);
		uint32_t font_width = max(6, font_height * 0.8);
		for (int i = 0; i < strlen(v->_title); i++) {
			draw_char(
				v->window->layer,
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
			v->frame.origin.x,
			rect_max_y(v->_title_inset)
		),
		size_make(
			v->frame.size.width,
			v->frame.size.height - v->_title_inset.size.height 
		)
	);

	draw_rect(
		v->window->layer, 
		outer_margin,
		color_light_gray(),
		outer_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	draw_rect(
		v->window->layer, 
		v->frame,
		outline_color,
		1
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
	draw_rect(
		v->window->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	int inset_adjustment_x = 3;
	// Top left corner
	draw_line(
		v->window->layer,
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
		v->window->layer,
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
		v->window->layer,
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
		v->window->layer,
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

	// Draw the inner content layer
	blit_layer(
		v->window->layer,
		v->content_layer,
		v->content_layer_frame,
		rect_make(
			point_zero(),
			v->content_layer_frame.size
		)
	);

	if (!v->controls_content_layer) {
		draw_rect(
			v->content_layer, 
			rect_make(
				point_zero(), 
				v->content_layer_frame.size
			), 
			v->background_color, 
			THICKNESS_FILLED
		);
		for (uint32_t j = 0; j < v->subviews->size; j++) {
			gui_elem_t* subview = array_lookup(v->subviews, j);

			//if (subview->base._priv_needs_display) {
				is_active = v->window->hover_elem == subview;
				subview->base._priv_draw_cb(subview, is_active);
				subview->base._priv_needs_display = false;
			//}
		}
	}
}

static void _view_window_resized(gui_view_t* v, Size new_window_size) {
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

	if (v->window_resized_cb) {
		v->window_resized_cb((gui_elem_t*)v, new_window_size);
	}
}

gui_view_t* gui_view_create(gui_window_t* window, gui_window_resized_cb_t sizer_cb) {
	gui_view_t* view = calloc(1, sizeof(gui_view_t));
	view->window = window;
	view->type = GUI_TYPE_VIEW;
	view->border_margin = 12;
	view->content_layer = create_layer(_gui_screen_resolution());

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
	view->sizer_cb = sizer_cb;

	view->subviews = array_create(32);

	view->frame = sizer_cb((gui_elem_t*)view, window->size);
	view->content_layer_frame = rect_make(point_zero(), view->frame.size);
	view->background_color = color_black();
	gui_view_set_title(view, NULL);

	printf("%s Initial view frame\n", rect_print(view->frame));

	array_insert(window->views, view);
	array_insert(window->all_gui_elems, view);

	return view;
}

void gui_view_destroy(gui_view_t* view) {
	layer_teardown(view->content_layer);
	array_destroy(view->subviews);
	free(view);
}
