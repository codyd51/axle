#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef uint8_t vga_color;
typedef uint16_t vga_entry;

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

static vga_color vga_make_color_selection(vga_text_mode_color foreground, vga_text_mode_color background) {
	return (vga_color)(foreground | (background << 4));
}

static vga_entry vga_entry_make(unsigned char ch, vga_color color) {
   return (uint16_t)ch | ((uint16_t)color << 8);
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len] != '\0') {
		len++;
	}
	return len;
}

size_t terminal_row;
size_t terminal_col;
vga_color terminal_color;
uint16_t* terminal_buffer;

void vga_term_init(void) {
	terminal_row = 0;
	terminal_col = 0;
	terminal_color = vga_make_color_selection(VGA_TEXT_MODE_COLOR_GREEN, VGA_TEXT_MODE_COLOR_BLACK);
	terminal_buffer = (uint16_t*)0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry_make(' ', terminal_color);
		}
	}
}

void vga_term_setcolor(vga_color col) {
	terminal_color = col;
}

void vga_term_place_char(unsigned char ch, vga_color color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry_make(ch, terminal_color);
}

void vga_term_putchar(unsigned char ch) {
	vga_term_place_char(ch, terminal_color, terminal_row, terminal_col);
	terminal_col++;
	if (terminal_col >= VGA_WIDTH) {
		terminal_col = 0;
		terminal_row++;
		if (terminal_row >= VGA_HEIGHT) {
			// TODO(PT): ran out of screen space. implement scrolling :)
			terminal_row = 0;
		}
	}
}

void vga_term_write(const char* str, size_t len) {
	for (size_t i = 0; i < len; i++) {
		// TODO(PT) add check for hitting null before len
		vga_term_putchar(str[i]);
	}
}

void vga_term_puts(const char* str) {
	vga_term_write(str, strlen(str));
}

void kernel_main(void) {
	vga_term_init();
	vga_term_puts("Hello world!\n");
}
