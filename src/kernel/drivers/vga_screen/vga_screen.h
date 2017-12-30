#ifndef VGA_SCREEN_H
#define VGA_SCREEN_H

#include <stdint.h>
#include <stddef.h>

/* Represents half of a text-mode color selection
 */
typedef enum vga_text_mode_color {
	VGA_TEXT_MODE_COLOR_BLACK = 0,
	VGA_TEXT_MODE_COLOR_BLUE = 1,
	VGA_TEXT_MODE_COLOR_GREEN = 2,
	VGA_TEXT_MODE_COLOR_CYAN = 3,
	VGA_TEXT_MODE_COLOR_RED = 4,
	VGA_TEXT_MODE_COLOR_MAGENTA = 5,
	VGA_TEXT_MODE_COLOR_BROWN = 6,
	VGA_TEXT_MODE_COLOR_LIGHT_GRAY = 7,
	VGA_TEXT_MODE_COLOR_DARK_GRAY = 8,
	VGA_TEXT_MODE_COLOR_LIGHT_BLUE = 9,
	VGA_TEXT_MODE_COLOR_LIGHT_GREEN = 10,
	VGA_TEXT_MODE_COLOR_LIGHT_CYAN = 11,
	VGA_TEXT_MODE_COLOR_LIGHT_RED = 12,
	VGA_TEXT_MODE_COLOR_LIGHT_MAGENTA = 13,
	VGA_TEXT_MODE_COLOR_LIGHT_BROWN = 14,
	VGA_TEXT_MODE_COLOR_WHITE = 15,
} vga_text_mode_color;

/* Low nibble specifies foreground color, high nibble specifies background color
 */
typedef uint8_t vga_screen_color;

/* Write the null-terminated string pointed to by `str` to the text-mode VGA buffer.
 */
void vga_screen_puts(const char* str);

/* Write the ASCII character `ch` to the text-mode VGA buffer
 */
void vga_screen_putc(unsigned char ch);

void vga_screen_init(void);

#endif
