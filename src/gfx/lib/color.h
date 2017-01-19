#ifndef COLOR_H
#define COLOR_H

#include <std/common.h>

typedef struct color {
	uint8_t val[4];
} Color;

typedef struct gradient {
	Color from;
	Color to;
} Gradient;

Color color_make(uint8_t red, uint8_t green, uint8_t blue);
uint32_t color_hex(Color color);

Gradient gradient_make(Color from, Color to);
Color color_at_ratio(Gradient gradient, double percent);

Color color_red();
Color color_orange();
Color color_yellow();
Color color_green();
Color color_blue();
Color color_purple();
Color color_brown();
Color color_black();
Color color_grey();
Color color_gray();
Color color_dark_grey();
Color color_dark_gray();
Color color_light_grey();
Color color_light_gray();
Color color_white();

#endif
