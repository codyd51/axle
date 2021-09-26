#include "terminal.h"
#include <kernel/assert.h>
#include <kernel/util/mutex/mutex.h>
#include <std/std.h>
#include <std/ctype.h>
#include <std/math.h>
#include <kernel/util/kbman/kbman.h>

/// Shared lock to keep all terminal operations atomic
static lock_t* mutex;

/// Combines a foreground and background color
typedef union rawcolor {
	struct {
		term_color fg : 4;
		term_color bg : 4;
	};
	uint8_t raw;
} rawcolor;

/// Internal structure of the terminal buffer
typedef union term_display {
	uint16_t grid[TERM_HEIGHT][TERM_WIDTH];
	uint16_t mem[TERM_AREA];
} term_display;

/// Current position of the terminal cursor
static struct term_cursor g_cursor_pos;

/// Current foreground and background color for newly written text
static rawcolor g_terminal_color;

/// Screen buffer for the terminal
static term_display* const g_terminal_buffer = (term_display*)0xB8000;

static void push_back_line(void);
static void newline(void);
static void putraw(char ch);
static void backspace(void);
static rawcolor make_color(term_color fg, term_color bg);
static uint16_t make_terminal_entry(uint8_t ch, rawcolor color);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"
static void update_cursor(term_cursor loc) {
	//TODO: implement it
}
#pragma GCC diagnostic pop

void terminal_initialize(void) {
	//initialize shared lock
	mutex = lock_create();

	terminal_setcolor(TERM_DEFAULT_FG, TERM_DEFAULT_BG);
	terminal_clear();
}

void terminal_clear(void) {
	lock(mutex);

	terminal_setcursor((term_cursor){0, 0});
	for(int i = 0; i < TERM_AREA; i++) {
		//uint16_t blank = make_terminal_entry(' ', g_terminal_color);
		//g_terminal_buffer->mem[i] = blank;
		putraw(' ');
	}
	terminal_setcursor((term_cursor){0, 0});

	unlock(mutex);
}

static void push_back_line(void) {
	lock(mutex);

	// Move all lines up one. This won't clear the last line
	for(uint16_t y = 1; y < TERM_HEIGHT; y++) {
		memcpy(&g_terminal_buffer->grid[y-1][0],
		       &g_terminal_buffer->grid[y][0],
		       TERM_WIDTH * sizeof(g_terminal_buffer->grid[0][0]));
	}

	// Clear the last line
	uint16_t blank = make_terminal_entry(' ', g_terminal_color);
	for(uint16_t x = 0; x < TERM_WIDTH; x++) {
		g_terminal_buffer->grid[TERM_HEIGHT - 1][x] = blank;
	}

	unlock(mutex);
}


typedef struct term_cell_color {
	term_color fg;
	term_color bg;
} term_cell_color;

static void newline(void) {
	g_cursor_pos.x = 0;
	if(++g_cursor_pos.y >= TERM_HEIGHT) {
		push_back_line();
		g_cursor_pos.y = TERM_HEIGHT - 1;
	}
}

static void putraw(char ch) {
	lock(mutex);

	// Find where to draw the character
	uint16_t* entry = &g_terminal_buffer->grid[g_cursor_pos.y][g_cursor_pos.x];

	// Draw the character
	*entry = make_terminal_entry(ch, g_terminal_color);

	// Advance cursor to next valid position
	if(++g_cursor_pos.x >= TERM_WIDTH) {
		newline();
	}

	unlock(mutex);
}

static void backspace(void) {
	term_cursor new_pos;
	if(g_cursor_pos.x == 0) {
		if(g_cursor_pos.y == 0) {
			// Can't delete if we're at the first spot
			return;
		}

		// Go back to last column on previous line
		new_pos.x = TERM_WIDTH - 1;
		new_pos.y = g_cursor_pos.y - 1;
	}
	else {
		// Go back one character on this line
		new_pos.x = g_cursor_pos.x - 1;
		new_pos.y = g_cursor_pos.y;
	}

	// Draw a space over the previous character, then back up
	g_cursor_pos = new_pos;
	putraw(' ');
	g_cursor_pos = new_pos;
}

//TODO REWORK THIS
//IMPORTANT
//SPAGEHTTI UPSETTI
static bool matching_color;
static char col_code[3];
static int parse_idx;
void terminal_putchar(char ch) {
	if (matching_color) {
		parse_idx++;
		if (ch == ';' || parse_idx >= 5) {
			matching_color = false;
			term_color col = (term_color)atoi(col_code);
			terminal_settextcolor(col);
			return;
		}
		if (parse_idx >= 2 && isdigit(ch)) {
			strccat(col_code, ch);
		}
		return;
	}

	switch(ch) {
		// Newline
		case '\n':
			newline();
			break;

		// Tab
		case '\t': {
			uint16_t tab = TERM_TABWIDTH - (g_cursor_pos.x % TERM_TABWIDTH);
			while(tab--) {
				if(g_cursor_pos.x >= TERM_WIDTH) {
					// Wrap to new line
					newline();
					break;
				}

				// Draw spaces to make the tab
				putraw(' ');
			}
			break;
		}

		// Backspace
		case '\b':
			backspace();
			break;

		// Alarm
		case '\a':
			//TODO
			break;

		// Formfeed
		case '\f':
			terminal_clear();
			break;

		// Vertical tab
		case '\v':
			if(++g_cursor_pos.y >= TERM_HEIGHT) {
				push_back_line();
				g_cursor_pos.y = TERM_HEIGHT - 1;
			}
			break;

		// Color change
		case '\e':
			matching_color = true;
			//reset temp args for this color switch
			parse_idx = 0;
			memset(col_code, 0, 3);
			return;
			break;

		// Normal characters
		default:
			putraw(ch);
			break;
	}

	// Update displayed cursor position
	terminal_updatecursor();
}

void terminal_writestring(const char* str) {
	while(*str != '\0') {
		terminal_putchar(*str++);
	}
}

static rawcolor make_color(term_color fg, term_color bg) {
	rawcolor ret;
	ret.fg =  fg;
	ret.bg = bg;
	return ret;
}

static uint16_t make_terminal_entry(uint8_t ch, rawcolor color) {
	return (color.raw << 8) | ch;
}

void terminal_setcolor(term_color fg, term_color bg) {
	if (fg > 15 || bg > 15) return;

	g_terminal_color = make_color(fg, bg);
}

void terminal_settextcolor(term_color color) {
	terminal_setcolor(color, g_terminal_color.bg);
}

void terminal_setbgcolor(term_color color) {
	terminal_setcolor(g_terminal_color.fg, color);
}

term_cursor terminal_getcursor(void) {
	return g_cursor_pos;
}

void terminal_setcursor(term_cursor curs) {
	ASSERT(curs.x < TERM_WIDTH && curs.y < TERM_HEIGHT,
		"Cursor out of bounds: (%d, %d)", curs.x, curs.y);
	g_cursor_pos = curs;
}

void terminal_updatecursor(void) {
	uint16_t loc = g_cursor_pos.y * TERM_WIDTH + g_cursor_pos.x;
	outb(0x3D4, 14);       //tell VGA board we're setting high cursor byte
	outb(0x3D5, loc >> 8); //send high cursor byte
	outb(0x3D4, 15);       //tell VGA board we're setting the low cursor byte
	outb(0x3D5, loc);      //send low cursor byte
}

void terminal_movecursor(term_cursor loc) {
	terminal_setcursor(loc);
	terminal_updatecursor();
}
