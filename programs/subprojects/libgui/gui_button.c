#include <string.h>
#include <libagx/lib/shapes.h>
#include <libagx/font/font.h>

#include "gui_button.h"
#include "gui_view.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _set_needs_display(gui_button_t* b) {
	b->_priv_needs_display = true;
	b->superview->_priv_needs_display = true;
}

static void _gui_button_handle_mouse_entered(gui_button_t* b) {
	b->in_left_click = false;
	_set_needs_display(b);
}

static void _gui_button_handle_mouse_exited(gui_button_t* b) {
	b->in_left_click = false;
	_set_needs_display(b);
}

static void _gui_button_handle_mouse_left_click(gui_button_t* b, Point mouse_pos) {
	b->in_left_click = true;
	if (b->button_clicked_cb) {
		b->button_clicked_cb(b);
	}
	_set_needs_display(b);
}

static void _gui_button_handle_mouse_left_click_ended(gui_button_t* b, Point mouse_pos) {
	b->in_left_click = false;
	_set_needs_display(b);
}

static void _gui_button_draw(gui_button_t* b, bool is_active) {
	Rect r = b->frame;

	// Outer margin
	Rect outer_margin = r;
	int outer_margin_size = 6;

	Color outer_margin_color = color_make(60, 60, 60);
	//Color inner_margin_color = color_make(100, 100, 100);
	Color inner_margin_color = color_light_gray();
	Color diagonal_insets_color = color_make(100, 100, 100);
	Color text_color = color_black();
	Color outline_color = is_active ? color_white() : color_make(50, 50, 50);
	if (b->in_left_click) {
		/*
		outline_color = color_make(200, 200, 200);
		outer_margin_color = color_light_gray();
		inner_margin_color = color_make(60, 60, 60);
		diagonal_insets_color = color_make(50, 50, 50);
		*/
		//outline_color = color_make(200, 200, 200);
		outer_margin_color = color_light_gray();
		diagonal_insets_color = color_dark_gray();
		inner_margin_color = color_make(60, 60, 60);
		//text_color = color_dark_gray();
	}

	gui_layer_draw_rect(
		b->superview->content_layer,
		outer_margin,
		outer_margin_color,
		outer_margin_size
	);

	Rect inner_margin = rect_make(
		point_make(
			rect_min_x(outer_margin) + outer_margin_size,
			rect_min_y(outer_margin) + outer_margin_size
		),
		size_make(
			outer_margin.size.width - (outer_margin_size * 2),
			outer_margin.size.height - (outer_margin_size * 2)
		)
	);
	gui_layer_draw_rect(
		b->superview->content_layer,
		inner_margin,
		inner_margin_color,
		THICKNESS_FILLED
	);

	/*
	draw_diagonal_insets(
		b->superview->content_layer,
		outer_margin, 
		inner_margin, 
		diagonal_insets_color,
		outer_margin_size
	);
	*/
	// Draw diagonal lines indicating an outset
	int t = outer_margin_size;

	// Top left corner
	Rect outer = outer_margin;
	Rect inner = inner_margin;
	Color c = diagonal_insets_color;
	gui_layer_t* layer = b->superview->content_layer;
	Line l = line_make(
		point_make(
			outer.origin.x + 1,
			outer.origin.y
		),
		point_make(
			inner.origin.x + 1,
			inner.origin.y
		)
	);
	gui_layer_draw_line(layer, l, c, t/2);

	// Bottom left corner
	l = line_make(
		point_make(
			rect_min_x(outer) + 2,
			rect_max_y(outer) - 2
		),
		point_make(
			rect_min_x(inner) + 1,
			rect_max_y(inner) - 1
		)
	);
	gui_layer_draw_line(layer, l, c, t/2);

	// Top right corner
	l = line_make(
		point_make(
			rect_max_x(outer) - 2,
			rect_min_y(outer)
		),
		point_make(
			rect_max_x(inner) - 2,
			rect_min_y(inner)
		)
	);
	gui_layer_draw_line(layer, l, c, t/2);

	// Bottom right corner
	l = line_make(
		point_make(
			rect_max_x(outer) - 2,
			rect_max_y(outer) - 1
		),
		point_make(
			rect_max_x(inner) - 2,
			rect_max_y(inner) - 1
		)
	);
	gui_layer_draw_line(layer, l, c, t/2);

	uint32_t font_height = min(30, inner_margin.size.height / 4);
	uint32_t font_width = max(6, font_height * 0.8);

	uint32_t len = strlen(b->title);
	uint32_t drawn_width = len * font_width;

	Point cursor = point_make(
		// Align the left edge with where the visual bevel begins
		b->frame.origin.x + (b->frame.size.width / 2) - (drawn_width / 2),
		b->frame.origin.y + (b->frame.size.height / 2 ) - (font_height / 2)
	);
	for (int i = 0; i < len; i++) {
		gui_layer_draw_char(
			b->superview->content_layer,
			b->title[i],
			cursor.x,
			cursor.y,
			text_color,
			size_make(font_height, font_height)
		);
		cursor.x += font_width;
	}

	// Outline above outer margin
	gui_layer_draw_rect(
		b->superview->content_layer,
		outer_margin,
		outline_color,
		1
	);

	// Outline above inner margin
	gui_layer_draw_rect(
		b->superview->content_layer,
		inner,
		color_make(140, 140, 140),
		1
	);
}

static void _button_window_resized(gui_button_t* b, Size new_window_size) {
	b->frame = b->sizer_cb((gui_elem_t*)b, new_window_size);
	b->_priv_needs_display = true;
}

gui_button_t* gui_button_create(gui_view_t* superview, gui_window_resized_cb_t sizer_cb, char* title) {
	gui_button_t* button = calloc(1, sizeof(gui_button_t));
	button->window = superview->window;
	button->superview = superview;
	button->type = GUI_TYPE_BUTTON;

	button->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_gui_button_handle_mouse_entered;
	button->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_gui_button_handle_mouse_exited;
	button->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_noop;
	button->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_noop;
	button->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_gui_button_handle_mouse_left_click;
	button->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_gui_button_handle_mouse_left_click_ended;
	button->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_noop;
	button->_priv_key_down_cb = (gui_key_down_cb_t)_noop;
	button->_priv_key_up_cb = (gui_key_up_cb_t)_noop;
	button->_priv_draw_cb = (gui_draw_cb_t)_gui_button_draw;
	button->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_button_window_resized;
	button->_priv_needs_display = true;
	button->sizer_cb = sizer_cb;

	button->title = strdup(title);

	button->frame = sizer_cb((gui_elem_t*)button, button->window->size);

	array_insert(superview->subviews, button);

	return button;
}

void gui_button_destroy(gui_button_t* button) {
	// TODO(PT): Does the slider need to be removed from superview->subviews?
	free(button->title);
	free(button);
}
