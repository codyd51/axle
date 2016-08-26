#ifndef FONT_H
#define FONT_H

#include <std/common.h>
#include <gfx/lib/color.h>

int char_index(char ch);

struct screen_t;
typedef struct screen_t Screen;

void draw_char(Screen* screen, char ch, int x, int y, Color color);

#endif
