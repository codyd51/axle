#include <std/std.h>
#include <gfx/lib/gfx.h>
#include <gfx/lib/view.h>
#include "font8x8.h"
#include <std/math.h>

//TODO configurable SSAA factor?
#define SSAA_FACTOR 4

//check if a given pixel at x/y is set in character bitmap of font
static bool font_px_on(char ch, int x, int y) {
	char font_row = font8x8_basic[(int)ch][y];
	//is pixel at idx x in row at y on?
	if ((font_row >> x) & 1) {
		return true;
	}
	return false;
}

//generate supersampled bitmap of font character
static void generate_supersampled_map(uint32_t* supersample, char ch) {
	//memset(supersample, 0, sizeof(supersample));

	//for every col in SS bitmap
	for (int y = 0; y < CHAR_HEIGHT * SSAA_FACTOR; y++) {
		uint32_t ssaa_row = 0;

		//get row from sampled bitmap
		int font_row = font8x8_basic[(int)ch][y / SSAA_FACTOR];

		//for every row in SS bitmap
		for (int x = 0; x < CHAR_WIDTH * SSAA_FACTOR; x++) {
			//copy state of sampled bitmap
			if ((font_row >> (x / SSAA_FACTOR)) & 1) {
				ssaa_row |= (1 << x);
			}
		}
		supersample[y] = ssaa_row;
	}
}

void draw_char(ca_layer* layer, char ch, int x, int y, Color color, Size font_size) {
	Point p = point_make(x, y);
	if (p.x < 0 || p.y < 0 || p.x >= layer->size.width || p.y >= layer->size.height) return;

	//find scale factor of default font size to size requested
	int scale_x = font_size.width / CHAR_WIDTH;
	int scale_y = font_size.height / CHAR_HEIGHT;

	int* bitmap = font8x8_basic[(int)ch];

	uint32_t supersample[CHAR_HEIGHT * SSAA_FACTOR] = {0};
	generate_supersampled_map(supersample, ch);

	//TODO bg_color should adapt to actual background color of dest layer
	Color bg_color = color_white();

	for (int y = 0; y < font_size.height; y++) {
		//get the corresponding y of default font size
		int font_y = y / scale_y;
		if (font_y >= CHAR_HEIGHT) continue;

		for (int x = 0; x < font_size.width; x++) {
			//corresponding x of default font size
			int font_x = x / scale_x;

			//skip this pixel if it isn't set in font
			if (!font_px_on(ch, font_x, font_y)) continue;

			//antialiasing
			//holds number of 'on' pixels in supersampled region
			//'on' pixels / total pixel count in SSAA region = alpha of pixel to draw
			int on_count = 0;
			int total_count = 0;

			//go around all supersampled pixels adjacent to current and
			//(square of 9 pixels)
			//record any 'on' pixels in supersample
			for (int dx = -1; dx <= 1; dx++) {
				for (int dy = -1; dy <= 1; dy++) {
					int ssaa_x = (font_x * SSAA_FACTOR) + dx;
					int ssaa_y = (font_y * SSAA_FACTOR) + dy;

					uint32_t ssaa_row = supersample[ssaa_y];
					total_count++;
					if ((ssaa_row >> ssaa_x) & 1) {
						on_count++;
					}
				}
			}

			//'on' pixels / total pixel count in SSAA region = alpha of pixel to draw
			float alpha = (float)on_count / (float)total_count;
			//if (alpha) {
				Color avg_color = color;
				//set avg_color to color * alpha
				//this is a lerp of background color to text color, at alpha
				for (int i = 0; i < gfx_bpp(); i++) {
					avg_color.val[i] = lerp(bg_color.val[i], avg_color.val[i], alpha);
				}
				putpixel(layer, p.x + x, p.y + y, avg_color);
			//}
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

void draw_string(ca_layer* dest, char* str, Point origin, Color color, Size font_size) {
	//get a pointer to location of a web link in this string, if any
	char* link_loc = link_hueristic(str);

	int idx = 0;
	int x = origin.x;
	int y = origin.y;

	//scale font parameters to size requested
	int scale_x = font_size.width / CHAR_WIDTH;
	int scale_y = font_size.height / CHAR_HEIGHT;
	int padding_w = CHAR_PADDING_W * scale_x;
	int padding_h = CHAR_PADDING_H * scale_y;

	while (str[idx]) {
		bool inserting_hyphen = false;
		//do we need to break a word onto 2 lines?
		if ((x + font_size.width + padding_w + 1) >= dest->size.width) {
			//don't bother if it was puncutation anyways
			if (str[idx] != ' ') {
				inserting_hyphen = true;
			}
		}
		else if (str[idx] == '\n') {
			x = 0;

			//quit if going to next line would exceed view bounds
			if ((y + font_size.height + padding_h + 1) >= dest->size.height) break;
			y += font_size.height + padding_h;
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
			if ((y + font_size.height + padding_h + 1) >= dest->size.height) break;
			y += font_size.height + padding_h;

			continue;
		}

		draw_char(dest, str[idx], x, y, draw_color, font_size);

		x += font_size.width + padding_w;
		idx++;
	}
}
