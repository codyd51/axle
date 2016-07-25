#include "terminal.h"
#include <std/panic.h>
#include <kernel/util/mutex/mutex.h>
#include <std/std.h>

#define TERM_HISTORY_MAX 256

/// Shared lock to keep all terminal operations atomic
static lock_t* mutex;

/// Combines a foreground and background color
typedef union rawcolor {
	struct {
		uint8_t fg : 4;
		uint8_t bg : 4;
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

/// Buffer to keep track of terminal history, not just what's currently visible
mutable_array_t term_history;

//TODO add info
static bool is_scroll_redraw;

static void push_back_line(void);
static void newline(void);
static void putraw(char ch);
static void backspace(void);
static rawcolor make_color(term_color fg, term_color bg);
static uint16_t make_terminal_entry(char ch, rawcolor color);
static void update_cursor(term_cursor loc);
static void term_end_line();

void terminal_initialize(void) {
	//initialize shared lock
	mutex = lock_create();

	is_scroll_redraw = false;
	term_history = array_m_create(TERM_HISTORY_MAX);

	//set up first line buffer
	term_end_line();
	
	terminal_setcolor(TERM_DEFAULT_FG, TERM_DEFAULT_BG);
	terminal_clear();
}

void terminal_clear(void) {
	lock(mutex);

	for(int i = 0; i < TERM_AREA; i++) {
		uint16_t blank = make_terminal_entry(' ', g_terminal_color);
		g_terminal_buffer->mem[i] = blank;
	}
	
	terminal_setcursor((term_cursor){0, 0});
	
	unlock(mutex);
}

static void push_back_line(void) {
	lock(mutex);
	
	// Move all lines up one. This won't clear the last line
	
	/*
	memmove(&g_terminal_buffer->mem[0],
	        &g_terminal_buffer->mem[TERM_WIDTH],
	        TERM_COUNT - TERM_WIDTH);
	*/
	
	// memmove is not currently implemented, so do this for now
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

static void term_end_line() {
	if (is_scroll_redraw) return;

	//if history is at capacity, dump the oldest line
	if (term_history.size == TERM_HISTORY_MAX - 1) {
		array_m_remove(0, &term_history);
	}

	char* new = (char*)kmalloc(sizeof(char) * TERM_WIDTH);
	array_m_insert(new, &term_history);
}

static void term_record_char(char ch) {
	if (is_scroll_redraw) return;

	//add this character to line buffer
	char* current = array_m_lookup(term_history.size - 1, &term_history);
	strccat(current, ch);
}

static void term_record_backspace() {
	if (is_scroll_redraw) return;

	// remove backspaced character
	// remove space rendered in place of backspaced character
	char* current = array_m_lookup(term_history.size - 1, &term_history);
	current[strlen(current) - 2] = '\0';
}

static void newline(void) {
	//flush the current line to terminal history
	term_end_line();

	g_cursor_pos.x = 0;
	if(++g_cursor_pos.y >= TERM_HEIGHT) {
		push_back_line();
		g_cursor_pos.y = TERM_HEIGHT - 1;
	}
}

static void putraw(char ch) {
	lock(mutex);

	term_record_char(ch);

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

	term_record_backspace();
}

void terminal_putchar(char ch) {
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
	return (rawcolor){
		.fg = fg,
		.bg = bg
	};
}

static uint16_t make_terminal_entry(char ch, rawcolor color) {
	return (color.raw << 8) | ch;
}

void terminal_setcolor(term_color fg, term_color bg) {
	g_terminal_color = make_color(fg, bg);
}

void terminal_settextcolor(term_color color) {
	g_terminal_color.fg = color;
}

void terminal_setbgcolor(term_color color) {
	g_terminal_color.bg = color;
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

typedef struct term_scroll_state {
	int height;
} term_scroll_t;

term_scroll_t scroll_state;

void term_scroll(term_scroll_direction dir) {
	lock(mutex);

	if (dir == TERM_SCROLL_UP) {
		if (scroll_state.height + TERM_HEIGHT == term_history.size) return;

		scroll_state.height++;
	} else {
		if (scroll_state.height == 0) return;

		scroll_state.height--;
	}
	
	is_scroll_redraw = true;
	terminal_clear();	
	for (int y = TERM_HEIGHT - 1; y >= 0; y--) {
		char* line = array_m_lookup(term_history.size - 1 - y - scroll_state.height, &term_history);
		printf("%s", line);
		if (y > 0) printf("\n");
	}
	is_scroll_redraw = false;

	unlock(mutex);
}
