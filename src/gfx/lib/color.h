#ifndef COLOR_H
#define COLOR_H

#include <std/common.h>

typedef struct color {
	uint8_t val[3];
} Color;

typedef struct gradient {
	Color* from;
	Color* to;
} Gradient;

Color* color_make(uint8_t red, uint8_t green, uint8_t blue);
uint32_t color_hex(Color* color);

Gradient* gradient_make(Color* from, Color* to);
Color* color_at_ratio(Gradient* gradient, double percent);

#endif
