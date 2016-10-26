#ifndef FONT_H
#define FONT_H

#include <std/common.h>
#include <gfx/lib/color.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING_W 0
#define CHAR_PADDING_H 6

typedef struct ca_layer_t ca_layer;
void draw_char(ca_layer* layer, char ch, int x, int y, Color color);

#endif
