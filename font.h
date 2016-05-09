#ifndef FONT_H
#define FONT_H

#include "gfx.h"
#include "common.h"
typedef struct {
	int rows[8];
} char_t;

typedef struct {
	char_t* characters[25];
} font_t;

int char_index(char ch);

font_t* setup_font();
int is_bit_set(int c, int n);
void draw_char(screen_t* screen, font_t* font_map, char ch);

#endif
