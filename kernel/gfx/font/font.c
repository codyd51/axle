#include <std/std.h>
#include <gfx/lib/gfx.h>
#include <std/math.h>

#include "font8x8.h"
#include "font.h"

//TODO configurable SSAA factor?
//#define SSAA_FACTOR 4
#define SSAA_FACTOR (0)
//#define SSAA_FACTOR 1

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

static uint32_t supersample_map_cache[256][CHAR_WIDTH * SSAA_FACTOR];
//generate supersampled bitmap of font character
static void generate_supersampled_map(uint32_t* supersample, char ch) {
	//bitset of characters which are stored in cache
	//256 / sizeof(uint32_t) = 8
	//so we need 8 uint32_t's to store all 256 possible characters
	static uint32_t present_in_cache[8] = {0};

	//use cached map if existing
	uint32_t* cached = (uint32_t*)&supersample_map_cache[(int)ch];
	if (bitset_check((uint32_t*)&present_in_cache, (int)ch)) {
		memcpy(supersample, cached, CHAR_WIDTH * SSAA_FACTOR * sizeof(uint32_t));
		return;
	}
	printk("generate_supersampled_map() didn't hit cache\n");

	//for every col in SS bitmap
	for (int y = 0; y < CHAR_HEIGHT * SSAA_FACTOR; y++) {
		uint32_t ssaa_row = 0;

		//get row from sampled bitmap
		int factor = MAX(SSAA_FACTOR, 1);
		int font_row = font8x8_basic[(int)ch][y / factor];

		//for every row in SS bitmap
		for (int x = 0; x < CHAR_WIDTH * SSAA_FACTOR; x++) {
			//copy state of sampled bitmap
			if ((font_row >> (x / factor)) & 1) {
				ssaa_row |= (1 << x);
			}
		}
		supersample[y] = ssaa_row;
		//save this row in cache
		cached[y] = ssaa_row;
	}
	//mark as saved in bitset
	bitset_set((uint32_t*)&present_in_cache, (int)ch);
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
	//const int factor = 6;
	//return size_make(s.width / factor, s.height / factor);
	return size_make(0, 0);
}

int visual_lines_for_string(char* str, Size font_size, Size render_region) {
	int count = 0;
	//int characters_per_line = ceil(render_region.width / (float)font_size.width);
	float characters_per_line = render_region.width / font_size.width;
	int line_index = 0;
	while (*str) {
		if (line_index >= characters_per_line || *str == '\n') {
			count++;
			line_index = 0;
		}
		line_index++;
		str++;
	}
	return count;
}

void draw_string(ca_layer* dest, char* str, Point origin, Color color, Size font_size) {
	int x = origin.x;
	int y = origin.y;
	Size padding = font_padding_for_size(font_size);

	//if the size of the string to draw is larger than the area to draw in, 
	//'scroll' the string and only draw the last visible area
	//TODO scroll behavior should be configurable!
	//first, we need to figure out how much space the string would take to display
	float characters_per_line = dest->size.width / font_size.width;
	int string_len = strlen(str);
	int lines_to_render = visual_lines_for_string(str, font_size, dest->size);
	float max_lines_possible = dest->size.height / font_size.height;

	//printk("characters_per_line %f string_len %d lines_to_render %f max_lines_possible %fstring %s\n", characters_per_line, string_len, lines_to_render, max_lines_possible, str);
	int idx = 0;

	if (lines_to_render > max_lines_possible) {
		int lines_to_skip = lines_to_render - max_lines_possible;
		idx += (characters_per_line * lines_to_skip);
		//printk("draw_string skipping first %d lines of text\n", lines_to_skip);
		if (idx >= string_len) {
			printk("draw_string scrolled past end of text\n");
			return;
		}
	}

	while (str[idx]) {
		bool inserting_hyphen = false;
		//do we need to break a word onto 2 lines?
		if ((x + font_size.width + padding.width + 1) >= dest->size.width) {
			int word_len = 0;
			for (int i = idx; i >= 0; i--) {
				if (!isalnum(str[i])) {
					break;
				}
				word_len++;
			}
			//if string is too short to hypenate, just add a newline
			//don't bother if it was puncutation anyways
			if (!isalnum(str[idx])) {
				inserting_hyphen = true;
			}
		}
		else if (str[idx] == '\n') {
			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + font_size.height + padding.height + 1) >= dest->size.height) break;
			y += font_size.height + padding.height;
			idx++;
			continue;
		}

		Color draw_color = color;

		if (inserting_hyphen) {
			draw_char(dest, '-', x, y, draw_color, font_size);
			//drew hyphen, continue without drawing current character in input string

			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + font_size.height + padding.height+ 1) >= dest->size.height) break;
			y += font_size.height + padding.height;

			continue;
		}

		draw_char(dest, str[idx], x, y, draw_color, font_size);

		x += font_size.width + padding.width;
		idx++;
	}
}

