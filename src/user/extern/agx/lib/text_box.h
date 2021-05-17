#ifndef TEXT_BOX_H
#define TEXT_BOX_H

#include "ca_layer.h"
#include "size.h"
#include "point.h"
#include "color.h"
#include "elem_stack.h"
#include "hash_map.h"

typedef struct array array_t;
typedef struct ca_scrolling_layer {
	ca_layer* layer;
	Size full_size;
	Size scroll_offset;
} ca_scrolling_layer_t;

typedef struct text_box {
	uint32_t scroll_position;
	ca_scrolling_layer_t* scroll_layer;
	Rect scrollbar_frame;
	ca_layer* scrollbar_layer;
	Size size;
	Size font_size;
	Size font_padding;
	Point text_inset;
	Point cursor_pos;
	Color background_color;
	uint32_t max_text_y;

	bool scrollable;

	elem_stack_t* history;
	bool preserves_history;

	bool cache_drawing;
	hash_map_t* draw_cache;

} text_box_t;

text_box_t* text_box_create(Size size, Color background_color);
text_box_t* text_box_create__unscrollable(Size size, Color background_color);
void text_box_destroy(text_box_t* text_box);

void text_box_putchar(text_box_t* text_box, char ch, Color color);
void text_box_puts(text_box_t* text_box, const char* str, Color color);

void text_box_set_cursor(text_box_t* text_box, Point point);
void text_box_set_cursor_x(text_box_t* text_box, uint32_t x);
void text_box_set_cursor_y(text_box_t* text_box, uint32_t y);

void text_box_clear(text_box_t* tb);
void text_box_clear_and_erase_history(text_box_t* tb);

void text_box_scroll_up(text_box_t* text_box);
void text_box_scroll_down(text_box_t* text_box);
void text_box_scroll_to_bottom(text_box_t* text_box);

void text_box_blit(text_box_t* text_box, ca_layer* dest, Rect dest_frame);
void text_box_resize(text_box_t* text_box, Size new_size);

#endif