#include <memory.h>
#include <stdlib.h>

#include <stdlibadd/array.h>
#include <stdlibadd/assert.h>

#include "text_box.h"
#include "shapes.h"
#include "../font/font.h"

#define SCROLL_INTERVAL 25

// From libport

#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a <= _b ? _a : _b; })

// Scrolling layer

ca_scrolling_layer_t* ca_scrolling_layer_create(Size full_size) {
	ca_scrolling_layer_t* layer = calloc(1, sizeof(ca_scrolling_layer_t));
	layer->layer = create_layer(full_size);
	layer->full_size = full_size;
	layer->scroll_offset = size_zero();
	return layer;
}

void ca_scrolling_layer_teardown(ca_scrolling_layer_t* sl) {
	layer_teardown(sl->layer);
	free(sl);
}

void ca_scrolling_layer_blit(ca_scrolling_layer_t* source, Rect source_frame, ca_layer* dest, Rect dest_frame) {
	source_frame.origin.x += source->scroll_offset.width;
	source_frame.origin.y += source->scroll_offset.height;
	blit_layer(dest, source->layer, dest_frame, source_frame);
}

// Scrolling implementation

typedef struct text_box_line {
	char* str;
	Color color;
	Size font_size;
} text_box_line_t;

static void _draw_scrollbar_into_layer(text_box_t* text_box, ca_layer* dest, Rect dest_frame) {
	ca_layer* sb_layer = text_box->scrollbar_layer;
	Rect sb_frame = text_box->scrollbar_frame;

	// Draw the scroll bar background
	draw_rect(
		sb_layer,
		rect_make(point_zero(), sb_frame.size),
		color_dark_gray(),
		THICKNESS_FILLED
	);
	draw_rect(
		sb_layer,
		rect_make(point_zero(), sb_frame.size),
		color_black(),
		1
	);

	// Draw the scroll bar indicator
	float indicator_x_margin = sb_frame.size.width / 5.0;
	float indicator_y_margin = sb_frame.size.height / 80.0;
	Size indicator_size = size_make(
		sb_frame.size.width - (indicator_x_margin * 2),
		sb_frame.size.height / 4
	);

	uint32_t max_visible_height = text_box->size.height + text_box->scroll_layer->scroll_offset.height;
	uint32_t max_total_height = text_box->max_text_y;
	float viewport_percent = max_visible_height / (float)max_total_height;
	if (text_box->scroll_layer->scroll_offset.height == 0) {
		viewport_percent = 0.0;
	}
	if (max_visible_height >= text_box->max_text_y) {
		viewport_percent = 1.0;
	}

	float usable_height = sb_frame.size.height;
	// Accomodate margins
	usable_height -= indicator_y_margin * 2;
	// Don't let the scroll bar go off-screen - the usable height stops where it reaches the bottom
	usable_height -= indicator_size.height;

	Point indicator_origin = point_make(
		indicator_x_margin,
		indicator_y_margin + (usable_height * viewport_percent)
	);
	// Move the indicator by half its height so it appears in the center
	//indicator_origin.y -= indicator_size.height / 2.0;

	// Draw the indicator
	draw_rect(
		sb_layer,
		rect_make(
			point_make(indicator_origin.x, indicator_origin.y), 
			indicator_size
		),
		color_black(),
		1
	);
	draw_rect(
		sb_layer,
		rect_make(
			point_make(indicator_origin.x + 1, indicator_origin.y + 1), 
			size_make(indicator_size.width - 2, indicator_size.height - 2)
		),
		color_light_gray(),
		THICKNESS_FILLED
	);

	blit_layer(dest, text_box->scrollbar_layer, text_box->scrollbar_frame, rect_make(point_zero(), sb_layer->size));
}

void text_box_scroll_up(text_box_t* text_box) {
	text_box->scroll_layer->scroll_offset.height += SCROLL_INTERVAL;
}

void text_box_blit(text_box_t* text_box, ca_layer* dest, Rect dest_frame) {
	Rect local_frame = rect_make(point_zero(), text_box->size);
	// Draw the visible region of the scrolling layer
	ca_scrolling_layer_blit(text_box->scroll_layer, local_frame, dest, dest_frame);
	// Draw the scroll bar indicator
	_draw_scrollbar_into_layer(text_box, dest, dest_frame);
}

void text_box_scroll_down(text_box_t* text_box) {
	text_box->scroll_layer->scroll_offset.height -= SCROLL_INTERVAL;
	text_box->scroll_layer->scroll_offset.height = max(text_box->scroll_layer->scroll_offset.height, 0);
}

void text_box_scroll_to_bottom(text_box_t* text_box) {

}

static void _newline(text_box_t* text_box) {
	text_box->cursor_pos.x = 0;
	text_box->cursor_pos.y += text_box->font_size.height + text_box->font_padding.height;
	text_box->max_text_y = max(text_box->max_text_y, text_box->cursor_pos.y);

	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->scroll_layer->full_size.height) {
		printf("Text box exceeded scrolling layer, reset to top... (%d)\n", text_box->history->count);
		while (text_box->history->count) {
			text_box_line_t* line = stack_pop(text_box->history);
			free(line->str);
			free(line);
		}
		text_box_clear(text_box);
	}
}

typedef struct char_desc {
	char ch;
	Size font_size;
	Color draw_color;
} char_desc_t;

void text_box_putchar(text_box_t* text_box, char ch, Color color) {
	if (ch == '\n') {
		_newline(text_box);
		return;
	}

	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->scroll_layer->full_size.height) {
		return;
	}

	if (ch == '\t') {
		for (int i = 0; i < 4; i++) {
			text_box_putchar(text_box, ' ', color);
		}
		return;
	}
	if (text_box->cache_drawing) {
		char_desc_t desc = {
			.ch = ch, 
			.font_size = text_box->font_size, 
			.draw_color = color
		};
		bool d = false;
		ca_layer* cache_layer = hash_map_get(text_box->draw_cache, &desc, sizeof(char_desc_t), d);
		if (cache_layer == NULL) {
			cache_layer = create_layer(
				size_make(
					text_box->font_size.width * 2,
					text_box->font_size.height * 2
				)
			);
			draw_rect(cache_layer, rect_make(point_zero(), cache_layer->size), text_box->background_color, THICKNESS_FILLED);
			draw_char(cache_layer, desc.ch, 0, 0, desc.draw_color, desc.font_size);
			hash_map_put(text_box->draw_cache, &desc, sizeof(char_desc_t), cache_layer, d);
		}
		blit_layer(
			text_box->scroll_layer->layer, 
			cache_layer, 
			rect_make(
				text_box->cursor_pos,
				text_box->font_size
			),
			rect_make(point_zero(), text_box->font_size)
		);
	}
	else {
		draw_char(text_box->scroll_layer->layer, ch, text_box->cursor_pos.x, text_box->cursor_pos.y, color, text_box->font_size);
	}

	text_box->cursor_pos.x += text_box->font_size.width + text_box->font_padding.width;
	if (text_box->cursor_pos.x + text_box->font_size.width + text_box->font_padding.width >= text_box->size.width - 40) {
		_newline(text_box);
	}
}

static void _text_box_puts_and_add_to_history(text_box_t* text_box, const char* str, Color color, bool add_to_history) {
	if (add_to_history && text_box->preserves_history) {
		text_box_line_t* line = calloc(1, sizeof(text_box_line_t));
		line->str = strdup(str);
		line->color = color;
		line->font_size = text_box->font_size;
		stack_push(text_box->history, line);
	}
	for (int i = 0; i < strlen(str); i++) {
		text_box_putchar(text_box, str[i], color);
	}
}

void text_box_puts(text_box_t* text_box, const char* str, Color color) {
	_text_box_puts_and_add_to_history(text_box, str, color, true);
}

void text_box_resize(text_box_t* text_box, Size size) {
    text_box->size = size;
	// Determine the scrollbar size
	float scrollbar_x_margin = size.width / 100.0;
	float scrollbar_y_margin = size.height / 50.0;
	scrollbar_y_margin = max(scrollbar_y_margin, 10);
	int scrollbar_width = max(size.width / 40.0, 20);
	Size scrollbar_size = size_make(
		scrollbar_width, 
		size.height - (scrollbar_y_margin * 4)
	);
	text_box->scrollbar_frame = rect_make(
		point_make(
			size.width - scrollbar_width - scrollbar_x_margin, 
			scrollbar_y_margin
		), 
		scrollbar_size
	);

	if (text_box->scrollbar_layer) {
		layer_teardown(text_box->scrollbar_layer);
	}
	text_box->scrollbar_layer = create_layer(scrollbar_size);

	if (text_box->preserves_history) {
		text_box_clear(text_box);
		stack_elem_t* ptr = text_box->history->head;
		for (uint32_t i = 0; i < text_box->history->count; i++) {
			text_box_line_t* line = ptr->payload;
			Size orig = text_box->font_size;
			text_box->font_size = line->font_size;
			_text_box_puts_and_add_to_history(text_box, line->str, line->color, false);
			text_box->font_size = orig;
			ptr = ptr->next;
		}
	}
}

text_box_t* text_box_create(Size size, Color background_color) {
    text_box_t* tb = calloc(1, sizeof(text_box_t));
	Size scroll_layer_size = size_make(1920, 1080 * 4);
	// TODO(PT): Drop second param?
	tb->scroll_layer = ca_scrolling_layer_create(scroll_layer_size);
    tb->background_color = background_color;
	tb->font_size = size_make(8, 12);
	tb->font_padding = size_make(0, 6);
	tb->history = stack_create();

	tb->cache_drawing = true;
	tb->draw_cache = hash_map_create();

	text_box_resize(tb, size);
	draw_rect(tb->scroll_layer->layer, rect_make(point_zero(), scroll_layer_size), tb->background_color, THICKNESS_FILLED);

    return tb;
}


void text_box_clear(text_box_t* tb) {
	Size s = tb->scroll_layer->full_size;
	draw_rect(
		tb->scroll_layer->layer, 
		rect_make(point_zero(), tb->scroll_layer->full_size), 
		tb->background_color, 
		THICKNESS_FILLED
	);
	tb->scroll_layer->scroll_offset = size_zero();
	tb->cursor_pos = point_zero();
	tb->max_text_y = 0;
}

void text_box_destroy(text_box_t* text_box) {
	ca_scrolling_layer_teardown(text_box->scroll_layer);
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
