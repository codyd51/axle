#include <std/std.h>
#include <gfx/lib/gfx.h>
#include "font8x8.h"

#define CH_W 8
#define CH_H 8

void draw_char(Screen* screen, char ch, int x, int y, Color color) {
	Coordinate p = point_make(x, y);
	if (p.x < 0 || p.y < 0 || p.x >= screen->window->frame.size.width || p.y >= screen->window->frame.size.height) return;

	int* bitmap = font8x8_basic[ch];
	for (int i = 0; i < CH_H; i++) {
		char row = bitmap[i];
		for (int j = 0; j < CH_W; j++) {
			if ((row >> j) & 1) {
				putpixel(screen, p.x + j, p.y + i, color);
			}
		}
	}
}


