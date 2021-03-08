#include <memory.h>
#include <stdlib.h>

#include <stdlibadd/array.h>
#include <stdlibadd/assert.h>

#include "text_box.h"
#include "shapes.h"
#include "../font/font.h"

typedef struct text_box_line {
	char* text;
	uint32_t len;
	Color color;
} text_box_line_t;

// From libport

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

// Internal array wrapper that provides access control and size tracking

// Scrolling implementation

static bool _can_scroll(text_box_t* text_box, int scroll_offset) {
	// How many lines will be visible?
	uint32_t visible_line_count = text_box->size.height / (text_box->font_size.height + text_box->font_padding.height);
	uint32_t empty_gap = 1;
	uint32_t old_lines_to_redraw = min(max(0, (int32_t)(visible_line_count - empty_gap)), text_box->lines->size);
	int32_t start_idx = (int32_t)(text_box->lines->size - old_lines_to_redraw - scroll_offset);

	// Are we unable to scroll? 
	// (Either there's not enough content to scroll off-screen, or we're at the top content)
	if (old_lines_to_redraw == text_box->lines->size || start_idx < 0) {
		return false;
	}

	// We're able to scroll the provided offset
	return true;
}

static void _draw_scrollbar(text_box_t* text_box, int scroll_offset) {
	uint32_t visible_line_count = text_box->size.height / (text_box->font_size.height + text_box->font_padding.height);
	uint32_t empty_gap = 1;
	uint32_t old_lines_to_redraw = min(max(0, (int32_t)(visible_line_count - empty_gap)), text_box->lines->size);
	int32_t start_idx = (int32_t)(text_box->lines->size - old_lines_to_redraw - scroll_offset);

	// Draw the scroll bar container
	float scroll_bar_container_x_margin = text_box->size.width / 100.0;
	float scroll_bar_container_y_margin = text_box->size.height / 50.0;

	int scroll_bar_container_width = text_box->size.width / 40.0;
	scroll_bar_container_width = min(scroll_bar_container_width, 30);
	Size scroll_bar_container_size = size_make(
		scroll_bar_container_width, 
		text_box->size.height - (scroll_bar_container_y_margin * 2)
	);
	Point scroll_bar_container_origin = point_make(
		text_box->size.width - scroll_bar_container_size.width - scroll_bar_container_x_margin,
		scroll_bar_container_y_margin
	);

	// Filled container
	draw_rect(text_box->layer, 
			  rect_make(scroll_bar_container_origin, 
					    scroll_bar_container_size),
			  color_dark_gray(),
			  THICKNESS_FILLED);

	// And border line
	draw_rect(text_box->layer, 
			  rect_make(scroll_bar_container_origin, 
					    scroll_bar_container_size),
			  color_black(),
			  2);

	// Draw the scroll bar indicator
	float scroll_bar_indicator_x_margin = scroll_bar_container_size.width / 5.0;
	float scroll_bar_indicator_y_margin = scroll_bar_container_size.height / 80.0;
	Size scroll_bar_indicator_size = size_make(
		scroll_bar_container_size.width - (scroll_bar_indicator_x_margin * 2),
		scroll_bar_container_size.height / 4
	);

	float viewport_percent = (start_idx + (visible_line_count / 2.0)) / (float)text_box->lines->size;
	if (start_idx == 0) {
		viewport_percent = 0.0;
	}
	if (old_lines_to_redraw + start_idx >= text_box->lines->size) {
		viewport_percent = 1.0;
	}

	float usable_height = scroll_bar_container_size.height;
	// Accomodate margins
	usable_height -= scroll_bar_indicator_y_margin * 2;
	// Don't let the scroll bar go off-screen - the usable height stops where it reaches the bottom
	usable_height -= scroll_bar_indicator_size.height;

	Point scroll_bar_indicator_origin = point_make(
		scroll_bar_container_origin.x + scroll_bar_indicator_x_margin, 
		scroll_bar_container_origin.y + scroll_bar_indicator_y_margin + (usable_height * viewport_percent)
	);

	draw_rect(text_box->layer, 
			  rect_make(scroll_bar_indicator_origin, 
					    scroll_bar_indicator_size),
			  color_light_gray(),
			  THICKNESS_FILLED);

	draw_rect(text_box->layer, 
			  rect_make(scroll_bar_indicator_origin, 
					    scroll_bar_indicator_size),
			  color_black(),
			  1);
}

static void _redraw_visible_window(text_box_t* text_box, int scroll_offset) {
	uint32_t visible_line_count = text_box->size.height / (text_box->font_size.height + text_box->font_padding.height);
	uint32_t empty_gap = 1;
	uint32_t old_lines_to_redraw = min(max(0, (int32_t)(visible_line_count - empty_gap)), text_box->lines->size);
	int32_t start_idx = (int32_t)(text_box->lines->size - old_lines_to_redraw - scroll_offset);

	// Redraw the text view with the visible window
	draw_rect(text_box->layer, rect_make(point_zero(), text_box->size), text_box->background_color, THICKNESS_FILLED);
	text_box->cursor_pos = point_zero();

	//for (int i = 0; i < min(old_lines_to_redraw, (text_box->lines->size - start_idx)); i++) {
	for (int i = start_idx; i < start_idx + old_lines_to_redraw; i++) {
		text_box_line_t* line = array_lookup(text_box->lines, i);
		for (int j = 0; j < line->len; j++) {
			text_box_putchar(text_box, line->text[j], line->color);
		}
	}
	_draw_scrollbar(text_box, scroll_offset);
}

void text_box_scroll_up(text_box_t* text_box) {
	if (!_can_scroll(text_box, text_box->scroll_position + 1)) {
		return;
	}

	text_box->scroll_position += 1;
	_redraw_visible_window(text_box, text_box->scroll_position);
}

void text_box_scroll_down(text_box_t* text_box) {
	// If we're already at the bottom, don't scroll further
	if (text_box->scroll_position == 0) return;

	text_box->scroll_position -= 1;
	_redraw_visible_window(text_box, text_box->scroll_position);
}

void text_box_scroll_to_bottom(text_box_t* text_box) {
	text_box->scroll_position = 0;
	_redraw_visible_window(text_box, text_box->scroll_position);
}

static void _newline(text_box_t* text_box) {
	text_box->cursor_pos.x = 0;
	text_box->cursor_pos.y += text_box->font_size.height + text_box->font_padding.height;

	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->size.height) {
		// Only redraw if we're already at the bottom
		if (text_box->scroll_position == 0) {
			draw_rect(text_box->layer, rect_make(point_zero(), text_box->size), text_box->background_color, THICKNESS_FILLED);
			text_box->cursor_pos = point_zero();
			//_redraw_visible_window(text_box, 0);
		}
	}
}

void text_box_putchar(text_box_t* text_box, char ch, Color color) {
	if (ch == '\n') {
		_newline(text_box);
		return;
	}

	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->size.height) {
		return;
	}

	if (ch == '\t') {
		for (int i = 0; i < 4; i++) {
			text_box_putchar(text_box, ' ', color);
		}
		return;
	}
	draw_char(text_box->layer, ch, text_box->cursor_pos.x, text_box->cursor_pos.y, color, text_box->font_size);

	text_box->cursor_pos.x += text_box->font_size.width + text_box->font_padding.width;
	if (text_box->cursor_pos.x + text_box->font_size.width + text_box->font_padding.width >= text_box->size.width) {
		_newline(text_box);
	}
}

static void _text_box_clear_history(text_box_t* text_box) {
	// Don't bother calling _remove() as it'll do unnecessary work of
	// shifting the entire array each time an element is popped.
	// Instead, just free everything and zero out the size field.
	for (int i = 0; i < text_box->lines->size; i++) {
		text_box_line_t* t = array_lookup(text_box->lines, i);
		free(t);
	}
	text_box->lines->size = 0;
	text_box->scroll_position = 0;
	_redraw_visible_window(text_box, 0);
}

void text_box_puts(text_box_t* text_box, const char* str, Color color) {
	// Record this line in the history
	text_box_line_t* line = calloc(1, sizeof(text_box_line_t));
	uint32_t len = strlen(str);
	line->text = calloc(1, len+1);
	line->len = len;
	line->color = color;
	strncpy(line->text, str, len);

	array_insert(text_box->lines, line);

	// Only draw if we're not scrolled somewhere else
	if (text_box->scroll_position == 0) {
		for (int i = 0; i < strlen(str); i++) {
			text_box_putchar(text_box, str[i], color);
		}
	}
	else {
		// TODO(PT): This should only happen if there's a newline in the string
		//printf("Adjust scroll pointer %d\n", text_box->scroll_position);
		// Adjust the scroll pointer so we stay in a logical place
		text_box->scroll_position += 1;
	}

	// Drop the history if we're about to overflow
	if (text_box->lines->size >= (text_box->lines->max_size - 4)) {
		_text_box_clear_history(text_box);
	}
	
	_draw_scrollbar(text_box, text_box->scroll_position);
}

text_box_t* text_box_create(Size size, Color background_color) {
    text_box_t* tb = calloc(1, sizeof(text_box_t));
	tb->lines = array_create(1024*8);
    tb->layer = create_layer(size);
    tb->size = size;
	tb->font_size = size_make(8, 12);
	tb->font_padding = size_make(0, 6);
    tb->background_color = background_color;
    // Fill the background color to start off with
	text_box_clear(tb);
    return tb;
}

void text_box_clear(text_box_t* tb) {
	_text_box_clear_history(tb);
}

void text_box_destroy(text_box_t* text_box) {
	layer_teardown(text_box->layer);
	for (int i = 0; i < text_box->lines->size; i++) {
		text_box_line_t* line = array_lookup(text_box->lines, i);
		free(line);
	}
	array_destroy(text_box->lines);
	free(text_box);
}

void text_box_set_cursor(text_box_t* text_box, Point point) {
    text_box->cursor_pos = point;
}

void text_box_set_cursor_x(text_box_t* text_box, uint32_t x) {
    text_box->cursor_pos.x = x;
}

void text_box_set_cursor_y(text_box_t* text_box, uint32_t y) {
    text_box->cursor_pos.y = y;
}
