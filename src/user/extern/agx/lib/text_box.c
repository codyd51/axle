#include <memory.h>
#include "text_box.h"
#include "shapes.h"

static void _newline(text_box_t* text_box) {
	text_box->cursor_pos.x = 0;
	text_box->cursor_pos.y += text_box->font_size.height + text_box->font_padding.height;

	if (text_box->cursor_pos.y + text_box->font_size.height + text_box->font_padding.height >= text_box->size.height) {
		draw_rect(text_box->layer, rect_make(point_zero(), text_box->size), text_box->background_color, THICKNESS_FILLED);
		text_box->cursor_pos = point_zero();
	}
}

void text_box_putchar(text_box_t* text_box, char ch, Color color) {
	if (ch == '\n') {
		_newline(text_box);
		return;
	}
	draw_char(text_box->layer, ch, text_box->cursor_pos.x, text_box->cursor_pos.y, color, text_box->font_size);

	text_box->cursor_pos.x += text_box->font_size.width + text_box->font_padding.width;
	if (text_box->cursor_pos.x + text_box->font_size.width + text_box->font_padding.width >= text_box->size.width) {
		_newline(text_box);
		return;
	}
}

void text_box_puts(text_box_t* text_box, const char* str, Color color) {
    for (int i = 0; i < strlen(str); i++) {
        text_box_putchar(text_box, str[i], color);
    }
}

text_box_t* text_box_create(Size size, Color background_color) {
    text_box_t* tb = calloc(1, sizeof(text_box_t));
    tb->layer = create_layer(size);
    tb->size = size;
	tb->font_size = size_make(12, 12);
	tb->font_padding = size_make(2, 2);
    tb->background_color = background_color;
    // Fill the background color to start off with
    draw_rect(tb->layer, rect_make(point_zero(), size), background_color, THICKNESS_FILLED);
    return tb;
}

void text_box_destroy(text_box_t* text_box) {
	layer_teardown(text_box->layer);
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
