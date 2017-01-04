#include <std/std.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include "font8x8.h"

#define CH_W 8
#define CH_H 8

void draw_char(ca_layer* layer, char ch, int x, int y, Color color) {
	Coordinate p = point_make(x, y);
	if (p.x < 0 || p.y < 0 || p.x >= layer->size.width || p.y >= layer->size.height) return;

	int* bitmap = font8x8_basic[(int)ch];
	for (int i = 0; i < CH_H; i++) {
		char row = bitmap[i];
		for (int j = 0; j < CH_W; j++) {
			if ((row >> j) & 1) {
				putpixel(layer, p.x + j, p.y + i, color);
			}
		}
	}
}

//returns first pointer to a string that looks like a web link in 'str', 
//or NULL if none is found
static char* link_hueristic(char* str) {
	static char link_stubs[2][16] = {"http",
									 "www",
	};
	char* test = NULL;
	//check if this matches any link hints
	for (int i = 0; i < sizeof(link_stubs) / sizeof(link_stubs[0]); i++) {
		test = strstr(str, link_stubs[i]);
		//match?
		if (test) {
			break;
		}
	}
	return test;
}

void draw_string(ca_layer* dest, char* str, Coordinate origin, Color color) {
	//get a pointer to location of a web link in this string, if any
	char* link_loc = link_hueristic(str);

	int idx = 0;
	int x = origin.x;
	int y = origin.y;
	while (str[idx]) {
		bool inserting_hyphen = false;
		//do we need to break a word onto 2 lines?
		if ((x + CHAR_WIDTH + CHAR_PADDING_W + 1) >= dest->size.width) {
			//don't bother if it was puncutation anyways
			if (str[idx] != ' ') {
				inserting_hyphen = true;
			}
		}
		else if (str[idx] == '\n') {
			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + CHAR_HEIGHT + CHAR_PADDING_H + 1) >= dest->size.height) break;
			y += CHAR_HEIGHT + CHAR_PADDING_H;
		}

		//if this is a link, draw blue
		Color draw_color = color;
		//are we currently drawing a link?
		if (link_loc && idx >= (link_loc - str)) {
			//reset link state on whitespace
			if (isspace(str[idx])) {
				link_loc = NULL;
			}
			else {
				//blue for link!
				draw_color = color_make(0, 0, 0xEE);
				//underline
				draw_line(dest, line_make(point_make(x, CHAR_HEIGHT + y), point_make(x + CHAR_WIDTH, y + CHAR_HEIGHT)), draw_color, 1);
			}
		}

		if (inserting_hyphen) {
			draw_char(dest, '-', x, y, draw_color);
			//drew hyphen, continue without drawing current character in input string
			
			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + CHAR_HEIGHT + CHAR_PADDING_H + 1) >= dest->size.height) break;
			y += CHAR_HEIGHT + CHAR_PADDING_H;

			continue;
		}

		draw_char(dest, str[idx], x, y, draw_color);

		x += CHAR_WIDTH + CHAR_PADDING_W;
		idx++;
	}
}
