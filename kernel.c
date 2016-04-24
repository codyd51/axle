#include "kernel.h"
#include "shell.h"

uint8_t make_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}

uint16_t make_vgaentry(char c, uint8_t color) {
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize() {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = make_color(COLOR_WHITE, COLOR_BLUE);
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

void terminal_push_back_line() {
	terminal_column = 0;

    //move every character up by one row
	for (size_t row = 0; row < VGA_HEIGHT; row++) {
		for (size_t col = 0; col < VGA_WIDTH; col++) {
			size_t index = (row+1) * VGA_WIDTH + col;
			terminal_putentryat(terminal_buffer[index], terminal_color, col, row);
		}
	}
	terminal_row = VGA_HEIGHT-1;
}

void terminal_putchar(char c) {
	//check for newline character
	if (c == '\n') {
		terminal_column = 0;
		if (++terminal_row >= VGA_HEIGHT) {
			terminal_push_back_line();
		}
	}
	//tab character
	else if (c == '\t') {
		terminal_column += 4;
	}
	else {
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	}
	
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;

		if (++terminal_row == VGA_HEIGHT) {	
			terminal_push_back_line();
		}
	}
}

void terminal_removechar() {
	terminal_putentryat(' ', terminal_color, terminal_column-1, terminal_row);
	--terminal_column;
}

void terminal_writestring(const char* data) {
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		terminal_putchar(data[i]);
	}
}

//declared within std.c
extern void initmem();

#if defined(__cplusplus)
extern "C" //use C linkage for kernel_main
#endif
void kernel_main() {
	//initialize terminal interface
	terminal_initialize();

	//set up memory for malloc to use
	initmem();

	//set up keyboard handshake
	init_pics(0x20, 0x28);

	/*
	char* split[10];
	//
	for (int i = 0; split[i+1] == 0; i++) {
		split[i] = teststr;
	}
	char* c = split[0];
	int counter = 1;
	while (*c) {
		if (*c == ' ') {
			*c = '\0';
			char* str = split[counter];
			str++;
			//counter++;
		}
		else {
			c++;
		}
	}
	for (int i = 0; split[i+1] == 0; i++) {
		//terminal_writestring('\n');
		terminal_writestring(split[i]);
	}*/
	terminal_putchar(toupper('t'));
	terminal_putchar(toupper('e'));
	terminal_putchar(toupper('s'));
	terminal_putchar(toupper('t'));
	terminal_writestring("\n");
	terminal_putchar(tolower('B'));
	terminal_putchar(tolower('L'));
	terminal_putchar(tolower('A'));
	terminal_putchar(tolower('H'));


	init_shell();
	int exit_status = 1;
	while (1) {
		shell();
	}
}


















