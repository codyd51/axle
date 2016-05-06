#include "kernel.h"
#include "shell.h"
#include "clock.h"
#include "common.h"

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
	terminal_color = make_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000;

	terminal_clear();
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_settextcolor(enum vga_color col) {
	terminal_color = make_color(col, (terminal_color >> 4));
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
			uint8_t color = terminal_buffer[index] >> 8;
			terminal_putentryat(terminal_buffer[index], color, col, row);
		}
	}
	terminal_row = VGA_HEIGHT-1;
}

//updates hardware cursor
void move_cursor() {
	//screen is 80 characters wide
	u16int cursorLocation = terminal_row * 80 + terminal_column;
	outb(0x3D4, 14); //tell VGA board we're setting high cursor byte
	outb(0x3D5, cursorLocation >> 8); //send high cursor byte
	outb(0x3D4, 15); //tell VGA board we're setting the low cursor byte
	outb(0x3D5, cursorLocation); //send low cursor byte
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

	move_cursor();
}

void terminal_removechar() {
	terminal_putentryat(' ', terminal_color, terminal_column-1, terminal_row);
	--terminal_column;
	move_cursor();
}

void terminal_writestring(const char* data) {
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_clear() {
	terminal_row = 0;
	terminal_column = 0;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = make_vgaentry(' ', terminal_color);
		}
	}
}

//defined in assembly
extern void enable_A20();

void enter_protected() {
	//disable interrupts
	//asm("cli");

	//wait for keyboard controller to clear
	//if bottom bit is set, kb is busy
	while (inb(0x64) & 1 != 0)
		printf("kb status: %d", inb(0x64));

	int i = enableA20();
	printf("return status: %d\n", i);
}

void test_colors() {
	terminal_settextcolor(COLOR_WHITE);
	printf("Testing colors...");
	for (int i = 0; i < 16; i++) {
		terminal_settextcolor(i);
		printf("@");
	}
	printf("\n");
	terminal_settextcolor(COLOR_WHITE);
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

	//enable protected mode
	//enter_protected();

	//introductory message
	terminal_settextcolor(COLOR_LIGHT_GREY);
	printf("[AXLE OS v0.0.1]\n");

	//run color test
	test_colors();

	//set up keyboard driver
	init_kb();

	//wait for user to start shell
	terminal_settextcolor(COLOR_LIGHT_GREY);
	printf("Kernel has finished booting. Press any key to enter shell.\n");
	printf("%c\n", getchar());

	init_shell();
	int exit_status = 1;
	while (1) {
		shell();
	}
}


















