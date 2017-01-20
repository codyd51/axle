#ifndef FONT_H
#define FONT_H

#include <std/common.h>
#include <gfx/lib/color.h>
#include <gfx/lib/view.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING_W 0
#define CHAR_PADDING_H 6

//write an ASCII character to a ca_layer with given origin and color
void draw_char(ca_layer* layer, char ch, int x, int y, Color color, Size font_size);
//write the string pointed to by str to ca_layer starting at given origin
//draw_string natively provides hyphen and hyperlink formatting support
void draw_string(ca_layer* dest, char* str, Point origin, Color color, Size font_size);

Size font_padding_for_size(Size s);

#endif
