#ifndef TEXT_MODE_H
#define TEXT_MODE_H

#include <stdint.h>
#include <stddef.h>

/* Represents half of a text-mode color selection
 */
typedef enum text_mode_color {
    TEXT_MODE_COLOR_BLACK = 0,
    TEXT_MODE_COLOR_BLUE = 1,
    TEXT_MODE_COLOR_GREEN = 2,
    TEXT_MODE_COLOR_CYAN = 3,
    TEXT_MODE_COLOR_RED = 4,
    TEXT_MODE_COLOR_MAGENTA = 5,
    TEXT_MODE_COLOR_BROWN = 6,
    TEXT_MODE_COLOR_LIGHT_GRAY = 7,
    TEXT_MODE_COLOR_DARK_GRAY = 8,
    TEXT_MODE_COLOR_LIGHT_BLUE = 9,
    TEXT_MODE_COLOR_LIGHT_GREEN = 10,
    TEXT_MODE_COLOR_LIGHT_CYAN = 11,
    TEXT_MODE_COLOR_LIGHT_RED = 12,
    TEXT_MODE_COLOR_LIGHT_MAGENTA = 13,
    TEXT_MODE_COLOR_LIGHT_BROWN = 14,
    TEXT_MODE_COLOR_WHITE = 15,
} text_mode_color;

/* Low nibble specifies foreground color, high nibble specifies background color
 */
typedef uint8_t text_mode_color_component;

/* Write the null-terminated string pointed to by `str` to the text-mode VGA buffer.
 */
void text_mode_puts(const char* str);

/* Write the ASCII character `ch` to the text-mode VGA buffer
 */
void text_mode_putchar(unsigned char ch);

void text_mode_init(void);

#endif