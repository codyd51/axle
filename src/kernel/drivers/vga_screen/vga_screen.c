#include "vga_screen.h"

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
	for (size_t y = 0; y < VGA_SCREEN_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_SCREEN_WIDTH; x++) {
			const size_t index = y * VGA_SCREEN_WIDTH + x;
			screen_state.buffer[index] = vga_screen_entry_make(' ', screen_state.color);
		}
	}
}

void vga_screen_init() {
	screen_state.cursor_row = 0;
	screen_state.cursor_col = 0;
	screen_state.color = vga_screen_color_make(VGA_TEXT_MODE_COLOR_GREEN, VGA_TEXT_MODE_COLOR_BLACK);
	screen_state.buffer = (uint16_t*)0xB8000;
	
	vga_screen_clear();
}

static void vga_screen_setcolor(vga_screen_color col) {
	screen_state.color = col;
}

void vga_screen_place_char(unsigned char ch, vga_screen_color color, size_t x, size_t y) {
	const size_t index = y * VGA_SCREEN_WIDTH + x;
	screen_state.buffer[index] = vga_screen_entry_make(ch, screen_state.color);
}

void vga_screen_putchar(unsigned char ch) {
	vga_screen_place_char(ch, screen_state.color, screen_state.cursor_col, screen_state.cursor_row);
	screen_state.cursor_col++;
	if (screen_state.cursor_col >= VGA_SCREEN_WIDTH) {
		screen_state.cursor_col = 0;
		screen_state.cursor_row++;
		if (screen_state.cursor_row >= VGA_SCREEN_HEIGHT) {
			// TODO(PT): ran out of screen space. implement scrolling :)
			screen_state.cursor_row = 0;
		}
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
