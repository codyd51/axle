#ifndef TEXT_BOX_H
#define TEXT_BOX_H

#include "ca_layer.h"
#include "size.h"
#include "point.h"
#include "color.h"

typedef struct array array_t;
typedef struct text_box {
	array_t* lines;
	uint32_t scroll_position;
    ca_layer* layer;
	Size size;
	Size font_size;
	Size font_padding;
	Point cursor_pos;
	Color background_color;
} text_box_t;

text_box_t* text_box_create(Size size, Color background_color);
void text_box_destroy(text_box_t* text_box);

void text_box_putchar(text_box_t* text_box, char ch, Color color);
void text_box_puts(text_box_t* text_box, const char* str, Color color);

void text_box_set_cursor(text_box_t* text_box, Point point);
void text_box_set_cursor_x(text_box_t* text_box, uint32_t x);
void text_box_set_cursor_y(text_box_t* text_box, uint32_t y);

void text_box_clear(text_box_t* tb);

void text_box_scroll_up(text_box_t* text_box);
void text_box_scroll_down(text_box_t* text_box);
void text_box_scroll_to_bottom(text_box_t* text_box);

#endif