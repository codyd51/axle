#include "color.h"
#include "std/kheap.h"
#include <std/math.h>

Color color_make(uint8_t red, uint8_t green, uint8_t blue) {
	red = MAX(red, 0);
	red = MIN(red, 255);
	green = MAX(green, 0);
	green = MIN(green, 255);
	blue = MAX(blue, 0);
	blue = MIN(blue, 255);

	Color ret;
	ret.val[0] = red;
	ret.val[1] = green;
	ret.val[2] = blue;
	return ret;
}

uint32_t color_hex(Color color) {
	uint32_t ret = (color.val[0] << 16) | (color.val[1] << 8) | (color.val[2]);
	return ret;
}

Gradient gradient_make(Color from, Color to) {
	Gradient ret;
	ret.from = from;
	ret.to = to;
	return ret;
}

Color color_at_ratio(Gradient gradient, double percent) {
	return color_make(0, 0, 0);
}
