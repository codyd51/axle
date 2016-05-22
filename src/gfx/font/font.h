#ifndef FONT_H
#define FONT_H

#include <gfx/lib/gfx.h>
#include <std/common.h>
typedef struct {
	int rows[8];
} char_t;

typedef struct {
	char_t* characters[25];
} Font;

int char_index(char ch);

Font* setup_font();
int is_bit_set(int c, int n);
void draw_char(Screen* screen, Font* font_map, char ch, int x, int y, int color);
void draw_string(Screen* screen, Font* font_map, char* str, int x, int y, int color);

#endif
