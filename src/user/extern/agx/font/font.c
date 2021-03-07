#include <stdint.h>
#include <stdbool.h>

#include "../lib/point.h"
#include "../lib/color.h"
#include "../lib/putpixel.h"
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
	if (p.x < 0 || p.y < 0 || p.x >= layer->size.width || p.y >= layer->size.height) return;

	//find scale factor of default font size to size requested
	float scale_x = font_size.width / (float)CHAR_WIDTH;
	float scale_y = font_size.height / (float)CHAR_HEIGHT;

	if (p.x + font_size.width >= layer->size.width || p.y + font_size.height >= layer->size.height) return;

	Color bg_color = color_black();
	int avg_red, avg_grn, avg_blu, sum_num;
	avg_red = avg_grn = avg_blu = sum_num = 0;
	//bg_color is the average color of the rect represented by 
	//{{x, y}, {font_size.width, font_size.height}}
	for (int bg_y = y; bg_y < y + font_size.height; bg_y++) {
		for (int bg_x = x; bg_x < x + font_size.width; bg_x++) {
			uint32_t idx = ((bg_y * layer->size.width * gfx_bytes_per_pixel()) + (bg_x * gfx_bytes_per_pixel()));
			avg_red += layer->raw[idx + 2];
			avg_grn += layer->raw[idx + 1];
			avg_blu += layer->raw[idx + 0];
			sum_num++;
		}
	}
	avg_red /= sum_num;
	avg_grn /= sum_num;
	avg_blu /= sum_num;
	bg_color.val[0] = avg_red;
	bg_color.val[1] = avg_grn;
	bg_color.val[2] = avg_blu;

	for (int draw_y = 0; draw_y < font_size.height; draw_y++) {
		//get the corresponding y of default font size
		int font_y = draw_y / scale_y;

		for (int draw_x = 0; draw_x < font_size.width; draw_x++) {
			//corresponding x of default font size
			int font_x = draw_x / scale_x;

			//skip antialiasing?
			uint32_t font_row = font8x8_basic[(int)ch][font_y];

			Color draw_color = color_white();
			float a = 1 - ( 0.299 * bg_color.val[0] + 
					0.587 * bg_color.val[1] + 
					0.114 * bg_color.val[2])/255;
			if (a < 0.5) draw_color = color_black();
			draw_color = color;
			if ((font_row >> font_x) & 1) {
				putpixel(layer, x + draw_x, y + draw_y, draw_color);
			}
			/*
			else {
				putpixel(layer, x + draw_x, y + draw_y, bg_color);
			}
			*/
			continue;
		}
	}
}

Size font_padding_for_size(Size s) {
	const int factor = 6;
	return size_make(s.width / factor, s.height / factor);
}

/*
//returns first pointer to a string that looks like a web link in 'str', 
//or NULL if none is found
char* link_hueristic(char* str) {
	static char link_stubs[2][16] = {"http",
		"www",
	};
	char* test = NULL;
	//check if this matches any link hints
	for (uint32_t i = 0; i < sizeof(link_stubs) / sizeof(link_stubs[0]); i++) {
		test = strstr(str, link_stubs[i]);
		//match?
		if (test) {
			break;
		}
	}
	return test;
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
	//get a pointer to location of a web link in this string, if any
	//don't check if str is too short to be a URL
	char* link_loc = NULL;
	if (strlen(str) < 7) {
		link_loc = link_hueristic(str);
	}

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

		//if this is a link, draw blue
		Color draw_color = color;
		//are we currently drawing a link?
		if (link_loc && idx >= (link_loc - str)) {
			//reset link state on whitespace, newline, or if this is the last line
			if (isspace(str[idx]) || str[idx] == '\n' || str[idx+1] == '\0') {
				link_loc = NULL;
			}
			else {
				//blue for link!
				draw_color = color_make(0, 0, 0xEE);
				//underline
				//draw_hline_fast(dest, line_make(point_make(x, y), point_make(x+1, y)), draw_color, 1);
			}
		}

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
*/
