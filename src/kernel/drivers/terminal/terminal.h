#ifndef STD_TERMINAL_H
#define STD_TERMINAL_H

#include <std/std_base.h>
#include <stdint.h>

__BEGIN_DECLS

/// Hardware text mode color constants
typedef enum term_color {
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
} term_color;

/// Holds the screen position of a terminal cursor
typedef struct term_cursor {
	uint16_t x;
	uint16_t y;
} term_cursor;


/// Width of the terminal screen in characters
#define TERM_WIDTH       80

/// Height of the terminal screen in characters
#define TERM_HEIGHT      25

/// Total number of characters on-screen
#define TERM_AREA        (TERM_WIDTH * TERM_HEIGHT)

/// Width of a tab in spaces
#define TERM_TABWIDTH    4

/// Default text color of the terminal
#define TERM_DEFAULT_FG  COLOR_LIGHT_BLUE

/// Default background color of the terminal
#define TERM_DEFAULT_BG  COLOR_BLACK


/// Initializes the terminal by setting the default colors and clearing the screen
STDAPI void terminal_initialize(void);

/// Clears the screen and move cursor to start
STDAPI void terminal_clear(void);

/// Write a character to the terminal
/// @param ch Character to output
/// @note This function handles special characters such as tab, backspace, etc.
STDAPI void terminal_putchar(char ch);

/// Write a string of characters to the terminal
/// @param str String to output
STDAPI void terminal_writestring(const char* str);

/// Sets the foreground and background colors of the terminal
/// @param fg Foreground (text) color
/// @param bg Background color
STDAPI void terminal_setcolor(term_color fg, term_color bg);

/// Sets the foreground text color of the terminal
/// @param color Text color
STDAPI void terminal_settextcolor(term_color color);

/// Sets the background color of the terminal
/// @param color Background color
STDAPI void terminal_setbgcolor(term_color color);

/// Retrieves the current position of the cursor in the terminal
/// @return Position of the terminal cursor
STDAPI term_cursor terminal_getcursor(void);

/// Set the terminal writing cursor to a new position without changing the position of the displayed cursor
/// @param loc New position for the terminal cursor
/// @note Does NOT update the displayed cursor location
STDAPI void terminal_setcursor(term_cursor loc);

/// Causes the displayed location of the cursor to be updated
STDAPI void terminal_updatecursor(void);

/// Move the terminal cursor and update the displayed cursor
/// @param loc New position for the terminal cursor
STDAPI void terminal_movecursor(term_cursor loc);

__END_DECLS

#endif // STD_TERMINAL_H
