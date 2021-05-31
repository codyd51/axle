#include <stdint.h>
#include <stdbool.h>

#include "../lib/color.h"
#include "../lib/putpixel.h"
#include "../lib/shapes.h"
#include "font8x8.h"
#include "font.h"

int gfx_bytes_per_pixel() { return 4; }

#define BITS_IN_WORD (sizeof(uint32_t) * 8)
static inline void bitset_set(uint32_t* bitset, int idx) {
	//figure out which index in array of ints to use
	int word_idx = idx / BITS_IN_WORD;
	//get offset within word
	int offset = idx % BITS_IN_WORD;
	//turn on bit
	bitset[word_idx] |= 1 << offset;
}

static inline bool bitset_check(uint32_t* bitset, int idx) {
	//figure out which index in array of ints to use
	int word_idx = idx / BITS_IN_WORD;
	//get offset within word
	int offset = idx % BITS_IN_WORD;
	//check bit
	return ((bitset[word_idx] >> offset) & 1);
}

void draw_char(ca_layer* layer, char ch, int x, int y, Color color, Size font_size) {
	Point p = point_make(x, y);
	//if (p.x < 0 || p.y < 0 || p.x >= layer->size.width || p.y >= layer->size.height) return;

	//find scale factor of default font size to size requested
	float scale_x = font_size.width / (float)CHAR_WIDTH;
	float scale_y = font_size.height / (float)CHAR_HEIGHT;

	if (p.x + font_size.width >= layer->size.width || p.y + font_size.height >= layer->size.height) return;

	for (int draw_y = 0; draw_y < font_size.height; draw_y++) {
		//get the corresponding y of default font size
		int font_y = draw_y / scale_y;

		for (int draw_x = 0; draw_x < font_size.width; draw_x++) {
			//corresponding x of default font size
			int font_x = draw_x / scale_x;

			//skip antialiasing?
			uint32_t font_row = font8x8_basic[(int)ch][font_y];

			if ((font_row >> font_x) & 1) {
				putpixel(layer, x + draw_x, y + draw_y, color);
			}
		}
	}
}

Size font_padding_for_size(Size s) {
	const int factor = 6;
	return size_make(s.width / factor, s.height / factor);
}

