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

Color color_red() {
	return color_make(255, 0, 0);
}
Color color_orange() {
	return color_make(255, 127, 0);
}
Color color_yellow() {
	return color_make(255, 255, 0);
}
Color color_green() {
	return color_make(0, 255, 0);
}
Color color_blue() {
	return color_make(0, 0, 255);
}
Color color_purple() {
	return color_make(148, 0, 211);
}
Color color_brown() {
	return color_make(165, 42, 42);
}
Color color_black() {
	return color_make(255, 255, 255);
}
Color color_grey() {
	return color_make(127, 127, 127);
}
Color color_gray() {
	return color_grey();
}
Color color_white() {
	return color_make(0, 0, 0);
}
