#include "terminal.h"

uint8_t make_color(enum terminal_color fg, enum terminal_color bg) {
	return fg | bg << 4;
}

uint16_t make_terminal_entry(char c, uint8_t color) {
	uint16_t c16 = c;
	uint16_t color16 = color;
	return c16 | color16 << 8;
}

cursor cursor_pos;
uint8_t terminal_color;
uint16_t* terminal_buffer;

void terminal_initialize() {
	cursor_pos.x = 0;
	cursor_pos.y = 0;
	
	terminal_color = make_color(COLOR_LIGHT_BLUE, COLOR_BLACK);
	terminal_buffer = TERMINAL_MEM;

	terminal_clear();
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_settextcolor(enum terminal_color col) {
	terminal_color = make_color(col, (terminal_color >> 4));
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * TERMINAL_WIDTH + x;
	terminal_buffer[index] = make_terminal_entry(c, color);
}

void terminal_push_back_line() {
	cursor_pos.x = 0;

    //move every character up by one row
	for (size_t row = 0; row < TERMINAL_HEIGHT; row++) {
		for (size_t col = 0; col < TERMINAL_WIDTH; col++) {
			size_t index = (row+1) * TERMINAL_WIDTH + col;
			uint8_t color = terminal_buffer[index] >> 8;
			terminal_putentryat(terminal_buffer[index], color, col, row);
		}
	}
	cursor_pos.y = TERMINAL_HEIGHT-1;
}

void terminal_putchar(char c) {
	//check for newline character
	if (c == '\n') {
		cursor_pos.x = 0;
		if (++cursor_pos.y >= TERMINAL_HEIGHT) {
			terminal_push_back_line();
		}
	}
	//tab character
	else if (c == '\t') {
		cursor_pos.x += 4;
	}
	else {
		terminal_putentryat(c, terminal_color, cursor_pos.x, cursor_pos.y);
	}
	
	if (++cursor_pos.x == TERMINAL_WIDTH) {
		cursor_pos.x = 0;

		if (++cursor_pos.y == TERMINAL_HEIGHT) {	
			terminal_push_back_line();
		}
	}

	move_cursor();
}

void terminal_removechar() {
	terminal_putentryat(' ', terminal_color, cursor_pos.x-1, cursor_pos.y);
	--cursor_pos.x;
	move_cursor();
}

void terminal_writestring(const char* data) {
	size_t datalen = strlen(data);
	for (size_t i = 0; i < datalen; i++) {
		terminal_putchar(data[i]);
	}
}

void terminal_clear() {
	cursor_pos.x = 0;
	cursor_pos.y = 0;

	for (size_t y = 0; y < TERMINAL_HEIGHT; y++) {
		for (size_t x = 0; x < TERMINAL_WIDTH; x++) {
			const size_t index = y * TERMINAL_WIDTH + x;
			terminal_buffer[index] = make_terminal_entry(' ', terminal_color);
		}
	}
}

void set_cursor(cursor curs) {
	cursor_pos = curs;
	set_cursor_indicator(cursor_pos);
}

cursor get_cursor() {
	return cursor_pos;
}

//update hardware cursor
void set_cursor_indicator(cursor curs) {
	//screen is 80 characters wide
	uint16_t loc = curs.y * 80 + curs.x;
	outb(0x3D4, 14); //tell VGA board we're setting high cursor byte
	outb(0x3D5, loc >> 8); //send high cursor byte
	outb(0x3D4, 15); //tell VGA board we're setting the low cursor byte
	outb(0x3D5, loc); //send low cursor byte
	
}

void move_cursor() {
	set_cursor_indicator(cursor_pos);
}
