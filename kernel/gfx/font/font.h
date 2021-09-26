#ifndef FONT_H
#define FONT_H

#include <std/common.h>
#include <gfx/lib/color.h>
#include <gfx/lib/view.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 8
#define CHAR_PADDING_W 0
#define CHAR_PADDING_H 6

/**
 * @brief Write an ASCII character to a ca_layer with given origin and color
 * draw_char will also antialiase the character if SSAA_FACTOR is defined with a value between 1-4.
 * If draw_char antialiases, it uses supersample antialiasing. This function also caches supersample maps,
 * so they do not need to be regenerated each time a character is rendered.
 * It will also scale the default font to the size provided.
 * As a background for the drawn character, the average color of the rectangle bounding the character is sampled.
 * @warning Currently, this function will only draw in black or white, depending on the intensity of the average background color. This is to attempt to ensure maximum legibility.
 * @param layer The desired graphics layer to render to
 * @param ch Character to render
 * @param x x-origin to render at
 * @param y y-origin to render at
 * @param color Color to draw the character
 * @param font_size Size to render the character
 */
void draw_char(ca_layer* layer, char ch, int x, int y, Color color, Size font_size);

/**
 * @brief Write the string pointed to by @p str to ca_layer starting at given origin
 * This function also uses a link heuristic to highlight hyperlinks wherever they are detected.
 * However, it will not scan for hyperlinks if the input string is less than 7 characters.
 * The string will also be hyphenated wherever drawing further would exceed the bounds of @p layer.
 * @param dest The layer to render to
 * @param str String to render to the layer
 * @param origin Origin at which to begin drawing
 * @param Color Color to draw text with
 * @param font_size Font size to scale system font to
 */
void draw_string(ca_layer* dest, char* str, Point origin, Color color, Size font_size);

/**
 * @brief Find the optimal font padding for @p font_size
 * @param s Size to find suggested padding for
 * @return The suggested font padding
 */
Size font_padding_for_size(Size s);

#endif
