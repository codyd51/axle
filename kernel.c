#if !defined(__cplusplus)
#include <stdbool.h> //C doesn't have boolean type by default
#endif
#include <stddef.h>
#include <stdint.h>

//check if the compiler thinks we're targeting the wrong OS
#if defined(__linux__)
#error "You are not using a cross compiler! You will certainly run into trouble."
#endif

//OS only works for the 32-bit ix86 target
#if !defined(__i386__)
#error "OS must be compiled with a ix86-elf compiler."
#endif

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

uint8_t make_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}

uint16_t make_vgaentry(char c, uint8_t color) {
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

size_t strlen(const char* str) {
	size_t ret = 0;
	while (str[ret] != 0) {
		ret++;
	}
	return ret;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize() {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = make_color(COLOR_RED, COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = make_vgaentry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = make_vgaentry(c, color);
}

void terminal_putchar(char c) {
	//check for newline character
	if (c == '\n') {
	    terminal_row++;
	    terminal_column = 0;
	}
	else {
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	}

	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) {	
			terminal_row = 0;
		}
	}
}

void terminal_writestring(const char* data) {
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		terminal_putchar(data[i]);
	}
}

char* itoa(int i, char b[]) {
	char const digit[] = "0123456789";
	char* p = b;
	if (i < 0) {
		*p++ = '-';
		i *= -1;
	}
	int shifter = i;
	do {
		//move to where representation ends
		++p;
		shifter = shifter/10;
	} while(shifter);
	
	*p = '\0';
	
	do {
		//move back, inserting digits as we go
		*--p = digit[i%10];
		i = i/10;
	} while (i);
	return b;
}

#if defined(__cplusplus)
extern "C" //use C linkage for kernel_main
#endif

void kernel_main() {
	//initialize terminal interface
	terminal_initialize();
	
	//terminal_writestring("Hello world! This is a test.\nTest 2.\nOne more line\nAnd another\n");
	int upper_bound = 300;
	for (int i = 0; i <= upper_bound; i++) {
		char str[4];
		terminal_writestring("iteration ");
		terminal_writestring(itoa(i, str));
		terminal_writestring(".\n");
		terminal_writestring(itoa((upper_bound - i), str));
		terminal_writestring(" iterations to go.\n");
	}
}


















