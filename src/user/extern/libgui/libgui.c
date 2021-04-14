#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <libamc/libamc.h>
#include <stdlibadd/array.h>
#include <stdlibadd/assert.h>

#include <agx/lib/size.h>
#include <agx/lib/screen.h>
#include <agx/lib/shapes.h>
#include <agx/lib/putpixel.h>

#include <awm/awm.h>

#include "libgui.h"

/* Shims */

// From libport

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

static void _noop() {}

// Many graphics lib functions call gfx_screen() 
Screen _screen = {0};
Screen* gfx_screen() {
	return &_screen;
}

/* Windows */

gui_window_t* gui_window_create(uint32_t width, uint32_t height) {
	// Ask awm to make a window for us
	amc_msg_u32_3__send(AWM_SERVICE_NAME, AWM_REQUEST_WINDOW_FRAMEBUFFER, width, height);

	// And get back info about the window it made
	amc_message_t* receive_framebuf;
	amc_message_await(AWM_SERVICE_NAME, &receive_framebuf);
	uint32_t event = amc_msg_u32_get_word(receive_framebuf, 0);
	if (event != AWM_CREATED_WINDOW_FRAMEBUFFER) {
		printf("Invalid state. Expected framebuffer command\n");
	}
	uint32_t framebuffer_addr = amc_msg_u32_get_word(receive_framebuf, 1);

	printf("Received framebuffer from awm: %d 0x%08x\n", event, framebuffer_addr);
	uint8_t* buf = (uint8_t*)framebuffer_addr;

	// TODO(PT): Use an awm command to get screen info
	_screen.resolution = size_make(1920, 1080);
	_screen.physbase = (uint32_t*)0;
	_screen.bits_per_pixel = 32;
	_screen.bytes_per_pixel = 4;

	ca_layer* dummy_layer = calloc(1, sizeof(ca_layer));
	dummy_layer->size = _screen.resolution;
	dummy_layer->raw = (uint8_t*)framebuffer_addr;
	dummy_layer->alpha = 1.0;
    _screen.vmem = dummy_layer;

	gui_window_t* window = calloc(1, sizeof(gui_window_t));
	window->_interrupt_cbs = array_create(64);
	window->size = size_make(width, height);
	window->layer = dummy_layer;
	window->text_inputs = array_create(32);
	window->text_views = array_create(32);
	window->all_gui_elems = array_create(64);

	return window;
}

/* Text input */

static void _text_input_handle_mouse_entered(text_input_t* ti) {
	if (ti->mouse_entered_cb) {
		ti->mouse_entered_cb(ti);
	}
}

static void _text_input_handle_mouse_exited(text_input_t* ti) {
	if (ti->mouse_exited_cb) {
		ti->mouse_exited_cb(ti);
	}
}

static void _text_input_handle_mouse_moved(text_input_t* ti, Point mouse_pos) {
	if (ti->mouse_moved_cb) {
		ti->mouse_moved_cb(ti, mouse_pos);
	}
}

static void _text_input_handle_mouse_left_click(text_input_t* ti, Point mouse_pos) {
	// Don't handle scrolls
}

static void _text_input_handle_mouse_scrolled(text_input_t* ti, int8_t delta_z) {
	// Don't scroll text inputs
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

	draw_rect(
		ti->window->layer, 
		outer_margin,
		color_light_gray(),
		ti->text_box_margin - inner_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	draw_rect(
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
	draw_rect(
		ti->window->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	int inset_adjustment_x = 3;
	// Top left corner
	draw_line(
		ti->window->layer,
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
		ti->window->layer,
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
		ti->window->layer,
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
		ti->window->layer,
		line_make(
			point_make(
				rect_max_x(inner_margin) - inset_adjustment_x,
				rect_max_y(inner_margin) - 2
			),
			point_make(
				rect_max_x(inner_margin) - inner_margin_size - inset_adjustment_x,
				rect_max_y(inner_margin) - inner_margin_size - 2
			)
		),
		inset_color,
		inner_margin_size
	);

	// Draw the inner text box
	Rect text_box_frame = rect_make(
		point_make(
			inner_margin.origin.x + inner_margin_size,
			inner_margin.origin.y + inner_margin_size
		),
		ti->text_box->size
	);
	//printf("blit %d %d %d %d -> %d %d\n", text_box_frame.origin.x, text_box_frame.origin.y, text_box_frame.size.width, text_box_frame.size.height, ti->window->layer->size.width, ti->window->layer->size.height);
	text_box_blit(ti->text_box, ti->window->layer, text_box_frame);

	// Draw the input indicator
	uint32_t now = ms_since_boot();
	if (now >= ti->next_indicator_flip_ms) {
		ti->next_indicator_flip_ms = now + 800;
		ti->indicator_on = !ti->indicator_on;
	}
	if (ti->indicator_on) {
		uint32_t input_indicator_body_width = 4;
		Rect input_indicator = rect_make(
			point_make(
				rect_min_x(text_box_frame) + ti->text_box->cursor_pos.x,
				rect_min_y(text_box_frame) + ti->text_box->cursor_pos.y + 2
			),
			size_make(
				input_indicator_body_width, 
				ti->text_box->font_size.height - 4
			)
		);

		draw_rect(
			ti->window->layer,
			input_indicator,
			color_gray(),
			THICKNESS_FILLED
		);
		draw_rect(
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
		draw_rect(
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
	Rect new_frame = ti->sizer_cb(ti, new_window_size);
	ti->frame = new_frame;

	Size new_text_box_frame = size_make(
		new_frame.size.width - (ti->text_box_margin * 2),
		new_frame.size.height - (ti->text_box_margin * 2)
	);
	text_box_resize(ti->text_box, new_text_box_frame);
}

text_input_t* gui_text_input_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb) {
	text_input_t* text_input = calloc(1, sizeof(text_input_t));
	text_input->window = window;
	text_input->prompt = NULL;

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
	text_input->frame = frame;

	uint32_t initial_bufsize = 64;
	text_input->text = calloc(1, initial_bufsize);
	text_input->max_len = initial_bufsize;

	text_input->_priv_mouse_entered_cb = _text_input_handle_mouse_entered;
	text_input->_priv_mouse_exited_cb = _text_input_handle_mouse_exited;
	text_input->_priv_mouse_moved_cb = _text_input_handle_mouse_moved;
	text_input->_priv_mouse_dragged_cb = _noop;
	text_input->_priv_mouse_left_click_cb = _text_input_handle_mouse_left_click;
	text_input->_priv_mouse_left_click_ended_cb = _noop;
	text_input->_priv_mouse_scrolled_cb = _text_input_handle_mouse_scrolled;
	text_input->_priv_draw_cb = _text_input_draw;
	text_input->_priv_window_resized_cb = _text_input_window_resized;
	text_input->sizer_cb = sizer_cb;
	printf("text_input->sizer 0x%08x\n", text_input->sizer_cb);

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

/* Text Display */

static void _text_view_handle_mouse_entered(text_view_t* tv) {
	if (tv->mouse_entered_cb) {
		tv->mouse_entered_cb(tv);
	}
}

static void _text_view_handle_mouse_exited(text_view_t* tv) {
	if (tv->mouse_exited_cb) {
		tv->mouse_exited_cb(tv);
	}
}

static void _text_view_handle_mouse_moved(text_view_t* tv, Point mouse_pos) {
	if (tv->mouse_moved_cb) {
		tv->mouse_moved_cb(tv, mouse_pos);
	}
}

static void _text_view_handle_mouse_left_click(text_view_t* tv, Point mouse_pos) {
	/*
	Rect sb_frame = tv->text_box->scrollbar_frame;
	printf("sb_frame %d %d %d %d mouse_pos %d %d\n", sb_frame.origin.x, sb_frame.origin.y, sb_frame.size.width, sb_frame.size.height, mouse_pos.x, mouse_pos.y);
	if (rect_contains_point(sb_frame, mouse_pos)) {
		// Figure out where in the scrollbar was clicked
		Point local_mouse = point_make(
			mouse_pos.x - rect_min_x(sb_frame),
			mouse_pos.y - rect_min_y(sb_frame)
		);
		printf("local mouse %d %d\n", local_mouse.x, local_mouse.y);
		float click_percent = local_mouse.y / (float)sb_frame.size.height;
		uint32_t max_total_height = tv->text_box->max_text_y + tv->text_box->size.height;
		uint32_t new_scroll_position = max_total_height * click_percent;
		tv->text_box->scroll_position = new_scroll_position;
		tv->text_box->scroll_layer->scroll_offset.height = new_scroll_position;
		printf("new_scroll_pos 0x%08x\n", new_scroll_position);
	}
	*/
}

static void _text_view_handle_mouse_scrolled(text_view_t* tv, int8_t delta_z) {
	// If we've decided we can't scroll now, don't try to
	if (tv->scrollbar->hidden) {
		return;
	}

	uint32_t scroll_off = tv->text_box->scroll_layer->scroll_offset.height;
	uint32_t bottom_y = tv->text_box->max_text_y;
	tv->scrollbar->scroll_percent = scroll_off / (float)bottom_y;

	// Only scroll the interior text box if we're in-bounds of scrollable content
	//if (tv->scrollbar->scroll_percent >= 0.0 && tv->scrollbar->scroll_percent <= 1.0) {
		bool scroll_up = delta_z > 0;
		for (uint32_t i = 0; i < abs(delta_z); i++) {
			if (scroll_up) text_box_scroll_up(tv->text_box);
			else text_box_scroll_down(tv->text_box);
		}
	//}
}

static void _text_view_draw(text_view_t* tv, bool is_active) {
	// Update whether the scroll bar should be visible or not
	uint32_t bottom_y = tv->text_box->max_text_y;
	if (bottom_y < tv->text_box->size.height) {
		tv->scrollbar->hidden = true;
	}
	else {
		tv->scrollbar->hidden = false;
	}

	// Margin
	uint32_t inner_margin_size = 6;

	// Outer margin
	Rect outer_margin = rect_make(
		tv->frame.origin,
		size_make(
			tv->frame.size.width,
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

	// Draw the inner text box
	Rect text_box_frame = rect_make(
		point_make(
			inner_margin.origin.x + inner_margin_size,
			inner_margin.origin.y + inner_margin_size
		),
		tv->text_box->size
	);
	text_box_blit(tv->text_box, tv->window->layer, text_box_frame);
}

static void _text_view_window_resized(text_view_t* tv, Size new_window_size) {
	// printf("text_view_resized 0x%08x\n", tv->sizer_cb);
	Rect new_frame = tv->sizer_cb(tv, new_window_size);
	tv->frame = new_frame;

	Size new_text_box_frame = size_make(
		new_frame.size.width - (tv->text_box_margin * 2),
		new_frame.size.height - (tv->text_box_margin * 2)
	);
	text_box_resize(tv->text_box, new_text_box_frame);
}

static Rect _text_view_scrollbar_sizer(gui_scrollbar_t* sb, Size window_size) {
	// Parent is guaranteed to be a text view
	text_view_t* parent = sb->parent;
	Size parent_size = parent->frame.size;
	float scrollbar_x_margin = parent->text_box_margin * 1.5;
	float scrollbar_y_margin = parent->text_box_margin * 2;

	int scrollbar_width = 32;
	int scrollbar_height = parent_size.height - (parent->text_box_margin * 2) - (scrollbar_y_margin * 2);

	// TODO(PT): Elements should receive their parent's size instead of the window size
	Point local_origin = point_make(
		parent_size.width - scrollbar_width - scrollbar_x_margin,
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
	text_view_t* parent = sb->parent;
	uint32_t bottom_y = parent->text_box->max_text_y;
	uint32_t scroll_off = bottom_y * new_scroll_percent;
	parent->text_box->scroll_layer->scroll_offset.height = scroll_off;
}

text_view_t* gui_text_view_create(gui_window_t* window, Rect frame, Color background_color, gui_window_resized_cb_t sizer_cb) {
	text_view_t* text_view = calloc(1, sizeof(text_view_t));
	text_view->window = window;
	text_view->frame = frame;

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

	text_view->_priv_mouse_entered_cb = _text_view_handle_mouse_entered;
	text_view->_priv_mouse_exited_cb = _text_view_handle_mouse_exited;
	text_view->_priv_mouse_moved_cb = _text_view_handle_mouse_moved;
	text_view->_priv_mouse_dragged_cb = _noop;
	text_view->_priv_mouse_left_click_cb = _text_view_handle_mouse_left_click;
	text_view->_priv_mouse_left_click_ended_cb = _noop;
	text_view->_priv_mouse_scrolled_cb = _text_view_handle_mouse_scrolled;
	text_view->_priv_draw_cb = _text_view_draw;
	text_view->_priv_window_resized_cb = _text_view_window_resized;
	text_view->sizer_cb = sizer_cb;

	array_insert(window->text_views, text_view);
	array_insert(window->all_gui_elems, text_view);

	// Must be called after text_view is added to all_gui_elems to set up the Z-order correctly
	text_view->scrollbar = gui_scrollbar_create(
		window,
		text_view,
		rect_zero(),
		_text_view_scrollbar_sizer
	);
	text_view->scrollbar->scroll_position_updated_cb = _text_view_scroll_pos_updated;

	return text_view;
}

static void _gui_scrollbar_handle_mouse_exited(gui_scrollbar_t* sb) {
	sb->in_left_click = false;
}

static void _gui_scrollbar_handle_mouse_moved(gui_scrollbar_t* sb, Point mouse_pos) {
	if (sb->scroll_position_updated_cb && sb->in_left_click) {
		// TODO(PT): Save inner_frame on the scrollbar for calc here
		float percent = (mouse_pos.y - sb->frame.origin.y) / (float)sb->frame.size.height;
		percent = max(min(percent, 1.0), 0.0);
		sb->scroll_percent = percent;
		sb->scroll_position_updated_cb(sb, percent);

	}
}

static void _gui_scrollbar_handle_mouse_dragged(gui_scrollbar_t* sb, Point mouse_pos) {
	if (sb->scroll_position_updated_cb && sb->in_left_click) {
		// TODO(PT): Save inner_frame on the scrollbar for calc here
		float percent = (mouse_pos.y - sb->frame.origin.y) / (float)sb->frame.size.height;
		percent = max(min(percent, 1.0), 0.0);
		sb->scroll_percent = percent;
		sb->scroll_position_updated_cb(sb, percent);

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
	}
}

static void _gui_scrollbar_handle_mouse_left_click_ended(gui_scrollbar_t* sb, Point mouse_pos) {
	sb->in_left_click = false;
}

static void _gui_scrollbar_window_resized(gui_scrollbar_t* sb, Size new_window_size) {
	Rect new_frame = sb->sizer_cb(sb, new_window_size);
	sb->frame = new_frame;
}

static void _gui_scrollbar_draw(gui_scrollbar_t* sb, bool is_active) {
	// TODO(PT): Move "hidden" to shared fields
	if (sb->hidden) {
		return;
	}
	Rect sb_frame = sb->frame;
	Rect local_frame = rect_make(point_zero(), sb->frame.size);

	// Margin
	uint32_t margin_size = 8;
	uint32_t inner_margin_size = 4;

	// Outer margin
	Rect outer_margin = local_frame;
	draw_rect(
		sb->layer,
		local_frame,
		color_light_gray(),
		margin_size - inner_margin_size
	);

	// Outline above outer margin
	Color outline_color = is_active ? color_make(200, 200, 200) : color_dark_gray();
	draw_rect(
		sb->layer,
		local_frame,
		outline_color,
		1
	);

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
	draw_rect(
		sb->layer, 
		inner_margin,
		color_dark_gray(),
		inner_margin_size
	);

	// Draw diagonal lines indicating an inset
	Color inset_color = color_make(50, 50, 50);
	int inset_adjustment_x = 1;
	int inset_width = 2;
	// Top left corner
	draw_line(
		sb->layer,
		line_make(
			point_make(
				inner_margin.origin.x,
				inner_margin.origin.y
			),
			point_make(
				inner_margin.origin.x + inner_margin_size + inset_adjustment_x,
				inner_margin.origin.y + inner_margin_size + inset_adjustment_x
			)
		),
		inset_color,
		inset_width
	);

	// Bottom left corner
	draw_line(
		sb->layer,
		line_make(
			point_make(
				inner_margin.origin.x + inset_adjustment_x + 1,
				rect_max_y(inner_margin) - 1
			),
			point_make(
				inner_margin.origin.x + inner_margin_size + inset_adjustment_x,
				rect_max_y(inner_margin) - inner_margin_size - 1
			)
		),
		inset_color,
		inset_width
	);

	// Top left corner
	draw_line(
		sb->layer,
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
		inset_width
	);

	// Bottom left corner
	draw_line(
		sb->layer,
		line_make(
			point_make(
				rect_max_x(inner_margin) - inset_adjustment_x,
				rect_max_y(inner_margin) - 1
			),
			point_make(
				rect_max_x(inner_margin) - inner_margin_size,
				rect_max_y(inner_margin) - inner_margin_size - 1
			)
		),
		inset_color,
		inset_width
	);

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
	draw_rect(
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
	draw_rect(
		sb->layer,
		indicator_frame,
		color_make(100, 100, 100),
		THICKNESS_FILLED
	);
	Color indicator_outline_color = is_active ? color_make(200, 200, 200) : color_make(60, 60, 60);
	draw_rect(
		sb->layer,
		indicator_frame,
		outline_color,
		1
	);

	blit_layer(
		sb->window->layer, 
		sb->layer,
		sb_frame,
		local_frame
	);
}

gui_scrollbar_t* gui_scrollbar_create(gui_window_t* window, gui_elem_t* parent, Rect frame, gui_window_resized_cb_t sizer_cb) {
	gui_scrollbar_t* sb = calloc(1, sizeof(text_view_t));
	sb->parent = parent;
	sb->window = window;
	sb->frame = frame;

	sb->layer = create_layer(
		size_make(
			// Max possible width of a scrollbar
			50,
			// Max possible height of a scrollbar
			_screen.resolution.height
		)
	);

	sb->_priv_mouse_entered_cb = _noop;
	sb->_priv_mouse_exited_cb = _gui_scrollbar_handle_mouse_exited;
	sb->_priv_mouse_moved_cb = _gui_scrollbar_handle_mouse_moved;
	sb->_priv_mouse_dragged_cb = _gui_scrollbar_handle_mouse_dragged;
	sb->_priv_mouse_left_click_cb = _gui_scrollbar_handle_mouse_left_click;
	sb->_priv_mouse_left_click_ended_cb = _gui_scrollbar_handle_mouse_left_click_ended;
	sb->_priv_mouse_scrolled_cb = _noop;
	sb->_priv_draw_cb = _gui_scrollbar_draw;
	sb->_priv_window_resized_cb = _gui_scrollbar_window_resized;
	sb->sizer_cb = sizer_cb;

	array_insert(window->all_gui_elems, sb);

	return sb;
}

void gui_text_view_clear(text_view_t* text_view) {
	text_box_clear(text_view->text_box);
}

void gui_text_view_destroy(text_view_t* text_view) {
	text_box_destroy(text_view->text_box);
	free(text_view);
}

/* Event Loop */

static void _handle_key_down(gui_window_t* window, char ch) {
	// Decide which element to route the keypress to
	// For now, always direct it to the first text input
	// TODO(PT): Model cursor position and use it to display the active text box
	if (window->text_inputs->size == 0) {
		return;
	}
	text_input_t* active_text_input = array_lookup(window->text_inputs, 0);
	if (ch == '\b') {
		if (active_text_input->len > 0) {
			char deleted_char = active_text_input->text[active_text_input->len - 1];
			active_text_input->text[--active_text_input->len] = '\0';

			text_box_t* text_box = active_text_input->text_box;

			uint32_t drawn_char_width = text_box->font_size.width + text_box->font_padding.width;
			uint32_t delete_width = drawn_char_width;
			// If the last character was a tab character, we actually need to emplace 4 spaces
			if (deleted_char == '\t') {
				delete_width = drawn_char_width * 4;
			}

			// Move the cursor back before the deleted character
			text_box->cursor_pos.x -= delete_width;
			// Cover it up with a space
			text_box_putchar(text_box, ' ', text_box->background_color);
			// And move the cursor before the space
			text_box->cursor_pos.x -= drawn_char_width;
		}
	}
	else {
		// Draw the character into the text input
		if (active_text_input->len + 1 >= active_text_input->max_len) {
			uint32_t new_max_len = active_text_input->max_len * 2;
			printf("Resizing text input %d -> %d\n", active_text_input->max_len, new_max_len);
			active_text_input->text = realloc(active_text_input->text, new_max_len);
			active_text_input->max_len = new_max_len;
		}
		active_text_input->text[active_text_input->len++] = ch;
		text_box_putchar(active_text_input->text_box, ch, color_black());
	}

	// Inform the text input that it's received a character
	if (active_text_input->text_entry_cb != NULL) {
		active_text_input->text_entry_cb(active_text_input, ch);
	}
}

static void _handle_mouse_moved(gui_window_t* window, awm_mouse_moved_msg_t* moved_msg) {
	Point mouse_pos = point_make(moved_msg->x_pos, moved_msg->y_pos);
	// Iterate backwards to respect z-order
	for (int32_t i = window->all_gui_elems->size - 1; i >= 0; i--) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		if (rect_contains_point(elem->ti.frame, mouse_pos)) {
			// Was the mouse already inside this element?
			if (window->hover_elem == elem) {
				Rect r = elem->ti.frame;
				//printf("Move within hover elem 0x%08x %d %d in %d %d %d %d\n", elem, mouse_pos.x, mouse_pos.y, r.origin.x, r.origin.y, r.size.width, r.size.height);
				elem->ti._priv_mouse_moved_cb(elem, mouse_pos);
				return;
			}
			else {
				// Exit the previous hover element
				if (window->hover_elem) {
					printf("Mouse exited previous hover elem 0x%08x\n", window->hover_elem);
					window->hover_elem->ti._priv_mouse_exited_cb(window->hover_elem);
					window->hover_elem = NULL;
				}
				printf("Mouse entered new hover elem 0x%08x\n", elem);
				window->hover_elem = elem;
				elem->ti._priv_mouse_entered_cb(elem);
				return;
			}
		}
	}
}

static void _handle_mouse_dragged(gui_window_t* window, awm_mouse_moved_msg_t* moved_msg) {
	Point mouse_pos = point_make(moved_msg->x_pos, moved_msg->y_pos);
	// Iterate backwards to respect z-order
	if (window->hover_elem) {
		window->hover_elem->ti._priv_mouse_dragged_cb(window->hover_elem, mouse_pos);
	}
}

static void _handle_mouse_left_click(gui_window_t* window, Point click_point) {
	if (window->hover_elem) {
		printf("Left click on hover elem 0x%08x\n", window->hover_elem);
		window->hover_elem->ti._priv_mouse_left_click_cb(window->hover_elem, click_point);
	}
}

static void _handle_mouse_left_click_ended(gui_window_t* window, Point click_point) {
	if (window->hover_elem) {
		window->hover_elem->ti._priv_mouse_left_click_ended_cb(window->hover_elem, click_point);
	}
}

static void _handle_mouse_exited(gui_window_t* window) {
	// Exit the previous hover element
	if (window->hover_elem) {
		printf("Mouse exited previous hover elem 0x%08x\n", window->hover_elem);
		window->hover_elem->ti._priv_mouse_exited_cb(window->hover_elem);
		window->hover_elem = NULL;
	}
}

static void _handle_mouse_scrolled(gui_window_t* window, awm_mouse_scrolled_msg_t* msg) {
	if (window->hover_elem) {
		window->hover_elem->ti._priv_mouse_scrolled_cb(window->hover_elem, msg->delta_z);
	}
}

typedef struct int_descriptor {
	uint32_t int_no;
	gui_interrupt_cb_t cb;
} int_descriptor_t;

static void _handle_amc_messages(gui_window_t* window) {
	// Deduplicate multiple resize messages in one event-loop pass
	bool got_resize_msg = false;
	awm_window_resized_msg_t newest_resize_msg = {0};

	do {
		amc_message_t* msg;
		amc_message_await_any(&msg);

		// Allow libamc to handle watchdogd pings
		if (libamc_handle_message(msg)) {
			continue;
		}

		// Handle awm messages
		else if (!strncmp(msg->source, AWM_SERVICE_NAME, AMC_MAX_SERVICE_NAME_LEN)) {
			uint32_t event = amc_msg_u32_get_word(msg, 0);
			if (event == AWM_KEY_DOWN) {
				char ch = (char)amc_msg_u32_get_word(msg, 1);
				_handle_key_down(window, ch);
			}
			else if (event == AWM_MOUSE_MOVED) {
				awm_mouse_moved_msg_t* m = (awm_mouse_moved_msg_t*)msg->body;
				_handle_mouse_moved(window, m);
			}
			else if (event == AWM_MOUSE_DRAGGED) {
				awm_mouse_dragged_msg_t* m = (awm_mouse_dragged_msg_t*)msg->body;
				_handle_mouse_dragged(window, m);
			}
			else if (event == AWM_MOUSE_LEFT_CLICK) {
				awm_mouse_left_click_msg_t* m = (awm_mouse_left_click_msg_t*)msg->body;
				_handle_mouse_left_click(window, m->click_point);
			}
			else if (event == AWM_MOUSE_LEFT_CLICK_ENDED) {
				awm_mouse_left_click_ended_msg_t* m = (awm_mouse_left_click_ended_msg_t*)msg->body;
				_handle_mouse_left_click_ended(window, m->click_end_point);
			}
			else if (event == AWM_MOUSE_EXITED) {
				_handle_mouse_exited(window);
			}
			else if (event == AWM_MOUSE_SCROLLED) {
				awm_mouse_scrolled_msg_t* m = (awm_mouse_scrolled_msg_t*)msg->body;
				_handle_mouse_scrolled(window, m);
			}
			else if (event == AWM_WINDOW_RESIZED) {
				got_resize_msg = true;
				awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)msg->body;
				newest_resize_msg = *m;
			}
			continue;
		}

		// Dispatch any other message
		else {
			if (window->_amc_handler != NULL) {
				window->_amc_handler(window, msg);
			}
		}
	} while (amc_has_message());

	if (got_resize_msg) {
		awm_window_resized_msg_t* m = (awm_window_resized_msg_t*)&newest_resize_msg;
		window->size = m->new_size;
		for (uint32_t i = 0; i < window->all_gui_elems->size; i++) {
			gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
			elem->ti._priv_window_resized_cb(elem, window->size);
		}
	}
}

static void _process_amc_messages(gui_window_t* window) {
	if (window->_interrupt_cbs->size) {
		assert(window->_interrupt_cbs->size == 1, "Only 1 interrupt supported");
		int_descriptor_t* t = array_lookup(window->_interrupt_cbs, 0);
		bool awoke_for_interrupt = adi_event_await(t->int_no);
		if (awoke_for_interrupt) {
			t->cb(window, t->int_no);
			return;
		}
	}
	_handle_amc_messages(window);
}

static void _run_event_loop(gui_window_t* window) {
	uint32_t start = ms_since_boot();
	// Blit views so that we draw everything once before blocking for amc
	for (uint32_t i = 0; i < window->all_gui_elems->size; i++) {
		gui_elem_t* elem = array_lookup(window->all_gui_elems, i);
		bool is_active = window->hover_elem == elem;
		elem->ti._priv_draw_cb(elem, is_active);
	}
	uint32_t end = ms_since_boot();
	uint32_t t = end - start;
	if (t > 2) {
		printf("[%d] libgui draw took %dms\n", getpid(), t);
	}

	// Process any events sent to this service
	_process_amc_messages(window);

	// Ask awm to update the window
	amc_msg_u32_1__send(AWM_SERVICE_NAME, AWM_WINDOW_REDRAW_READY);
}

void gui_enter_event_loop(gui_window_t* window) {
	while (true) {
		_run_event_loop(window);
	}
}

void gui_add_interrupt_handler(gui_window_t* window, uint32_t int_no, gui_interrupt_cb_t cb) {
	//hash_map_put(window->_interrupt_cbs, &int_no, sizeof(uint32_t), cb);
	int_descriptor_t* d = calloc(1, sizeof(int_descriptor_t));
	d->int_no = int_no;
	d->cb = cb;
	array_insert(window->_interrupt_cbs, d);
}

void gui_add_message_handler(gui_window_t* window, gui_amc_message_cb_t cb) {
	if (window->_amc_handler != NULL) {
		assert(0, "Only one amc handler is supported");
	}
	window->_amc_handler = cb;
}
