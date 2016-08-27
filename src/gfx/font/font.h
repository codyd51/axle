#ifndef FONT_H
#define FONT_H

#include <std/common.h>
#include <gfx/lib/color.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING_W 0
#define CHAR_PADDING_H 6

struct screen_t;
typedef struct screen_t Screen;

void draw_char(Screen* screen, char ch, int x, int y, Color color);

#endif
