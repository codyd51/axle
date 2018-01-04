#include "vga_screen.h"
#include <std/ctype.h>

typedef uint16_t vga_screen_entry;
static const size_t VGA_SCREEN_WIDTH = 80;
static const size_t VGA_SCREEN_HEIGHT = 25;

static vga_screen_color vga_screen_color_make(vga_text_mode_color foreground, vga_text_mode_color background) {
	return (vga_screen_color)(foreground | (background << 4));
}

static vga_screen_entry vga_screen_entry_make(unsigned char ch, vga_screen_color color) {
   return (uint16_t)ch | ((uint16_t)color << 8);
}

typedef struct screen_state {
	size_t cursor_row;
	size_t cursor_col;
	vga_screen_color color;
	uint16_t* buffer;
} screen_state_t;
screen_state_t screen_state;

void vga_screen_clear() {
	screen_state.cursor_row = 0;
	screen_state.cursor_col = 0;
	for (size_t y = 0; y < VGA_SCREEN_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_SCREEN_WIDTH; x++) {
			const size_t index = y * VGA_SCREEN_WIDTH + x;
			screen_state.buffer[index] = vga_screen_entry_make(' ', screen_state.color);
		}
	}
}

static void vga_screen_setcolor(vga_screen_color col) {
	screen_state.color = col;
}

void vga_screen_init() {
	vga_screen_setcolor(vga_screen_color_make(VGA_TEXT_MODE_COLOR_WHITE, VGA_TEXT_MODE_COLOR_BLACK));
	screen_state.buffer = (uint16_t*)0xB8000;
	vga_screen_clear();
}

static void vga_screen_scroll_up_line(void) {
	for (size_t y = 0; y < VGA_SCREEN_HEIGHT - 1; y++) {
		for (size_t x = 0; x < VGA_SCREEN_WIDTH; x++) {
			const size_t index = y * VGA_SCREEN_WIDTH + x;
			//copy the data here to the spot 1 row above
			const size_t above_index = (y - 1) * VGA_SCREEN_WIDTH + x;
			screen_state.buffer[above_index] = screen_state.buffer[index];
		}
	}
	//empty bottom line
	const bottom_row = VGA_SCREEN_HEIGHT - 1;
	for (size_t x = 0; x < VGA_SCREEN_WIDTH; x++) {
		const size_t index = bottom_row * VGA_SCREEN_WIDTH + x;
		screen_state.buffer[index] = vga_screen_entry_make(' ', screen_state.color);
	}
}

static void vga_screen_newline(void) {
	screen_state.cursor_row++;
	screen_state.cursor_col = 0;

	if (screen_state.cursor_row >= VGA_SCREEN_HEIGHT) {
		vga_screen_scroll_up_line();
	}
}

static void vga_screen_tab(void) {
	const int tab_len = 4;
	for (int i = 0; i < tab_len; i++) {
		vga_screen_putchar(' ');
	}
}

void vga_screen_place_char(unsigned char ch, vga_screen_color color, size_t x, size_t y) {
	const size_t index = y * VGA_SCREEN_WIDTH + x;
	screen_state.buffer[index] = vga_screen_entry_make(ch, screen_state.color);
}

static void vga_screen_cursor_increment(void) {
	screen_state.cursor_col++;
	if (screen_state.cursor_col >= VGA_SCREEN_WIDTH) {
		vga_screen_newline();
	}
}

static void vga_screen_putchar_printable(unsigned char ch) {
	vga_screen_place_char(ch, screen_state.color, screen_state.cursor_col, screen_state.cursor_row);
	vga_screen_cursor_increment();
}

static void vga_screen_putchar_special(unsigned char ch) {
	// TODO(PT): verify ch is a special char!
	switch (ch) {
		case '\n':
			vga_screen_newline();
			break;
		case '\t':
			vga_screen_tab();
			break;
		default:
			break;
	}
}

void vga_screen_putchar(unsigned char ch) {
	if (isprint(ch)) {
		vga_screen_putchar_printable(ch);
	}
	else {
		vga_screen_putchar_special(ch);
	}
}

void vga_screen_write(const char* str, size_t len) {
	for (size_t i = 0; i < len; i++) {
		// TODO(PT) add check for hitting null before len
		vga_screen_putchar(str[i]);
	}
}

void vga_screen_puts(const char* str) {
	vga_screen_write(str, strlen(str));
}
