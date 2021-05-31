#include <string.h>
#include <agx/lib/shapes.h>
#include "gui_text_input.h"
#include "libgui.h"
#include "utils.h"

static void _noop() {}

static void _text_input_handle_mouse_entered(text_input_t* ti) {
	if (ti->mouse_entered_cb) {
		ti->mouse_entered_cb((gui_elem_t*)ti);
	}
	ti->_priv_needs_display = true;
}

static void _text_input_handle_mouse_exited(text_input_t* ti) {
	if (ti->mouse_exited_cb) {
		ti->mouse_exited_cb((gui_elem_t*)ti);
	}
	ti->_priv_needs_display = true;
}

static void _text_input_handle_mouse_moved(text_input_t* ti, Point mouse_pos) {
	if (ti->mouse_moved_cb) {
		ti->mouse_moved_cb((gui_elem_t*)ti, mouse_pos);
	}
	ti->_priv_needs_display = true;
}

static void _text_input_draw(text_input_t* ti, bool is_active) {
	// Margin
	uint32_t inner_margin_size = 6;

	// Outer margin
	Rect outer_margin = rect_make(
		ti->frame.origin,
		size_make(
			ti->frame.size.width,
			ti->frame.size.height
		)
	);

	gui_layer_draw_rect(
		ti->window->layer, 
		outer_margin,
		color_light_gray(),
		ti->text_box_margin - inner_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	gui_layer_draw_rect(
		ti->window->layer, 
		ti->frame,
		outline_color,
		1
	);

	uint32_t outer_margin_before_inner = ti->text_box_margin - inner_margin_size;
	Rect inner_margin = rect_make(
		point_make(
			rect_min_x(outer_margin) + ti->text_box_margin - inner_margin_size,
			rect_min_y(outer_margin) + ti->text_box_margin - inner_margin_size
		),
		size_make(
			outer_margin.size.width - (outer_margin_before_inner * 2),
			outer_margin.size.height - (outer_margin_before_inner * 2)
		)
	);
	gui_layer_draw_rect(
		ti->window->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	Rect text_box_frame = rect_make(
		point_make(
			inner_margin.origin.x + inner_margin_size,
			inner_margin.origin.y + inner_margin_size
		),
		ti->text_box->size
	);
	draw_diagonal_insets(
		ti->window->layer,
		inner_margin,
		text_box_frame,
		inset_color,
		inner_margin_size
	);

	// Draw the inner text box
	text_box_blit(ti->text_box, ti->window->layer->fixed_layer.inner, text_box_frame);

	// Draw the input indicator
	// TODO(PT): Replace this state machine with a timer
	uint32_t now = ms_since_boot();
	/*
	if (now >= ti->next_indicator_flip_ms) {
		ti->next_indicator_flip_ms = now + 800;
		ti->indicator_on = !ti->indicator_on;
	}
	*/
	ti->indicator_on = true;
	if (ti->indicator_on) {
		uint32_t input_indicator_body_width = max(2, ti->text_box->font_size.width / 4);
		Rect input_indicator = rect_make(
			point_make(
				rect_min_x(text_box_frame) + ti->text_box->cursor_pos.x + (input_indicator_body_width / 2),
				rect_min_y(text_box_frame) + ti->text_box->cursor_pos.y + 2
			),
			size_make(
				input_indicator_body_width, 
				ti->text_box->font_size.height - 4
			)
		);

		gui_layer_draw_rect(
			ti->window->layer,
			input_indicator,
			color_gray(),
			THICKNESS_FILLED
		);
		gui_layer_draw_rect(
			ti->window->layer,
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
			ti->window->layer,
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

static void _text_input_window_resized(text_input_t* ti, Size new_window_size) {
	Rect new_frame = ti->sizer_cb((gui_elem_t*)ti, new_window_size);
	ti->frame = new_frame;

	Size new_text_box_frame = size_make(
		new_frame.size.width - (ti->text_box_margin * 2),
		new_frame.size.height - (ti->text_box_margin * 2)
	);
	text_box_resize(ti->text_box, new_text_box_frame);
	ti->_priv_needs_display = true;
}

text_input_t* gui_text_input_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb) {
	text_input_t* text_input = calloc(1, sizeof(text_input_t));
	text_input->window = window;
	text_input->prompt = NULL;
    text_input->type = GUI_TYPE_TEXT_INPUT;

	text_input->text_box_margin = 12;
	text_box_t* text_box = text_box_create(
		size_make(
			frame.size.width - (text_input->text_box_margin * 2),
			frame.size.height - (text_input->text_box_margin * 2)
		),
		background_color
	);
	text_input->text_box = text_box;
	text_input->text_box->preserves_history = true;
	text_input->text_box->cache_drawing = true;

	uint32_t initial_bufsize = 64;
	text_input->text = calloc(1, initial_bufsize);
	text_input->max_len = initial_bufsize;

	text_input->_priv_mouse_entered_cb = (gui_mouse_entered_cb_t)_text_input_handle_mouse_entered;
	text_input->_priv_mouse_exited_cb = (gui_mouse_exited_cb_t)_text_input_handle_mouse_exited;
	text_input->_priv_mouse_moved_cb = (gui_mouse_moved_cb_t)_text_input_handle_mouse_moved;
	text_input->_priv_mouse_dragged_cb = (gui_mouse_dragged_cb_t)_noop;
	text_input->_priv_mouse_left_click_cb = (gui_mouse_left_click_cb_t)_noop;
	text_input->_priv_mouse_left_click_ended_cb = (gui_mouse_left_click_ended_cb_t)_noop;
	text_input->_priv_mouse_scrolled_cb = (gui_mouse_scrolled_cb_t)_noop;
	text_input->_priv_key_down_cb = (gui_key_down_cb_t)_noop;
	text_input->_priv_key_up_cb = (gui_key_up_cb_t)_noop;
	text_input->_priv_draw_cb = (gui_draw_cb_t)_text_input_draw;
	text_input->_priv_window_resized_cb = (_priv_gui_window_resized_cb_t)_text_input_window_resized;
	text_input->_priv_needs_display = true;
	text_input->sizer_cb = sizer_cb;
	text_input->frame = sizer_cb((gui_elem_t*)text_input, window->size);

	array_insert(window->text_inputs, text_input);
	array_insert(window->all_gui_elems, text_input);

	return text_input;
}

void gui_text_input_set_prompt(text_input_t* ti, char* prompt) {
	if (ti->prompt) {
		free(ti->prompt);
	}
	ti->prompt = strdup(prompt);
	gui_text_input_clear(ti);
}

void gui_text_input_clear(text_input_t* ti) {
	text_box_clear(ti->text_box);
	memset(ti->text, 0, ti->max_len);
	ti->len = 0;
	// Draw the input prompt, if one has been set up
	if (ti->prompt) {
		text_box_puts(ti->text_box, ti->prompt, color_gray());
	}
}

void gui_text_input_destroy(text_input_t* ti) {
	text_box_destroy(ti->text_box);
	free(ti->text);
	free(ti);
}
