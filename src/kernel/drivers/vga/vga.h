#ifndef VGA_H
#define VGA_H

#include <std/std.h>

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static uint16_t * const VGA_MEM = (uint16_t*)0xB8000;

//hardware text mode color constants
enum vga_color {
	COLOR_BLACK = 0,
	COLOR_BLUE = 1,
	COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_removechar();
void terminal_clear();
void terminal_settextcolor(enum vga_color col); 
void set_cursor(size_t col, size_t row);
size_t term_col();
size_t term_row();

#endif
