#include "printf.h"
#include <stdarg.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/drivers/serial/serial.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/drivers/pit/pit.h>
#include <std/string.h>
#include <gfx/lib/gfx.h>

char* convert(unsigned int num, int base) {
	static char representation[] = "0123456789ABCDEF";
	static char buffer[50];
	char* ptr;

	ptr = &buffer[49];
	*ptr = '\0';

	do {
		*--ptr = representation[num%base];
		num /= base;
	} while (num != 0);

	return (ptr);
}

enum {
	TERM_OUTPUT = 0,
	SERIAL_OUTPUT,
};

static void backspace(Point* cursor_pos) {
	Point new_pos = *cursor_pos;
	Size font_size = gfx_screen()->default_font_size;
	Size padding = font_padding_for_size(font_size);

	if(cursor_pos->x == 0) {
		if(cursor_pos->y == 0) {
			// Can't delete if we're at the first spot
			return;
		}

		// Go back to last column on previous line
		new_pos.x = gfx_screen()->resolution.width - font_size.width - padding.width;
		new_pos.y = cursor_pos->y - font_size.height - padding.height;
	}
	else {
		// Go back one character on this line
		new_pos.x = cursor_pos->x - font_size.width - padding.width;
	}

	// Draw a space over the previous character, then back up
	*cursor_pos = new_pos;
	putchar(' ');
	*cursor_pos = new_pos;
}

void scroll_display_up() {
	Screen* screen = gfx_screen();
	Size font_size = screen->default_font_size;

	//top line of text is lost
	Rect shifted = rect_make(point_make(0, font_size.height), size_make(screen->resolution.width, screen->resolution.height - font_size.height));
	//for each line in shifted, copy data upwards
	for (int y = rect_min_y(shifted); y <= rect_max_y(shifted); y++) {
		int overwrite_y = y - font_size.height;
		int overwrite_idx = (overwrite_y * screen->resolution.width * screen->bpp);
		int source_idx = (y * screen->resolution.width * screen->bpp);
		memcpy(screen->vmem->raw + overwrite_idx, screen->vmem->raw + source_idx, screen->resolution.width * screen->bpp);
	}
	write_screen(screen);
}

static Color printf_draw_color = {{0, 255, 0}};
static Point cursor_pos = {0, 0};

void reset_cursor_pos() {
	cursor_pos.x = 0;
	cursor_pos.y = 0;
}

void gfx_set_cursor_pos(int x, int y) {
	cursor_pos.x = x;
	cursor_pos.y = y;
}

Point gfx_get_cursor_pos() {
	return cursor_pos;
}

static void outputc(int dest, char c) {
	switch (dest) {
		case TERM_OUTPUT: {
			terminal_putchar(c);
			if (c == '\b') {
				backspace(&cursor_pos);
				return;
			}

			gfx_terminal_putchar(c);
			break;
		}
		case SERIAL_OUTPUT:
			serial_putchar(c);
		default:
			break;
	}
}

int putchar(int ch) {
	outputc(TERM_OUTPUT, (unsigned char)ch);
	return ch;
}

Color term_color_code(int code) {
	switch (code) {
		case 0: return color_black(); break;
		case 1: return color_blue(); break;
		case 2: return color_make(29, 224, 65); break;
		case 3: return color_make(29, 90, 224); break;
		case 4: return color_make(229, 29, 187); break;
		case 5: return color_purple(); break;
		case 6: return color_orange(); break;
		case 7: return color_light_gray(); break;
		case 8: return color_dark_gray(); break;
		case 9: return color_make(29, 90, 224); break;
		case 10: return color_make(29, 224, 65); break;
		case 11: return color_blue(); break;
		case 12: return color_make(229, 29, 187); break;
		case 13: return color_purple(); break;
		case 14: return color_yellow(); break;
		case 15: return color_white(); break;
		case 16: return color_black(); break;
		case 17: return color_dark_gray(); break;
		case 18: return color_dark_gray(); break;
		case 19: return color_gray(); break;
		case 20: return color_gray(); break;

		default: return color_red(); break;
	}
	return color_white();
}

void output(int dest, char* str) {
	if (!str) return;
	while (*str) {
		outputc(dest, *(str++));
	}
}

void print_hex_common(int dest, uint32_t n) {
	unsigned short tmp;
	output(dest, "0x");

	char noZeroes = 1;
	int i;
	bool leading_zeroes = true;
	for (i = 28; i > 0; i -= 4) {
		tmp = (n >> i) & 0xF;
		if (tmp == 0 && noZeroes != 0 && !leading_zeroes) {
			outputc(dest, '0');
			continue;
		}
		if (tmp >= 0xA) {
			leading_zeroes = false;
			noZeroes = 0;
			outputc(dest, tmp - 0xA + 'a');
		}
		else {
			noZeroes = 0;
			outputc(dest, tmp + '0');
		}
	}

	tmp = n & 0xF;
	if (tmp >= 0xA) {
		outputc(dest, tmp - 0xA + 'a');
	}
	else {
		outputc(dest, tmp + '0');
	}
}

void printf_hex(uint32_t n) {
	print_hex_common(TERM_OUTPUT, n);
}

void printk_hex(uint32_t n) {
	print_hex_common(SERIAL_OUTPUT, n);
}

void printk_debug_info() {
	char now[64];
	memset(now, 0, 64);
	date((char*)&now);
	//printk("[PID %d @ %s (tick %d)] ", getpid(), now, tick_count());
#include <kernel/util/multitasking/tasks/task.h>
	extern task_t* current_task;
	printk("[%s[%d] %d] ", (current_task) ? current_task->name : "no", getpid(), tick_count());
}

//keep track of when to print debug info
//only do so on newline
bool seen_newline = false;
void vprintf(int dest, char* format, va_list va) {
	if (dest == TERM_OUTPUT) {
		vprintf(SERIAL_OUTPUT, format, va);
	}

	char bf[24];
	char ch;

	while ((ch = *(format++))) {
		if (ch != '%') {
			if (ch == '\n') {
				seen_newline = true;
			}
			else if (dest == TERM_OUTPUT && ch == '\e' && *format == '[' && isdigit(*(format + 1))) {
				//skip \e
				//format++;
				//skip [
				format++;

				char buf[16] = {0};
				int digit_count = 0;
				while ((buf[digit_count++] = *(format++)) != ';') {
				}
				int as_num = atoi(buf);
				Color new_display_col = term_color_code(as_num);
				printf_draw_color = new_display_col;

				ch = *format;
				continue;
			}
			else if (ch == '\0') {
				output(dest, "(null)");
				continue;
			}
			outputc(dest, ch);
		}
		else {
			// char zero_pad; //TODO: make use of this
			char* ptr;

			ch = *(format++);

			//zero padding requested
			if (ch == '0') {
				ch = *(format++);
				if (ch == '\0') return;
				if (ch >= '0' && ch <= '9') {
					// zero_pad = ch - '0';
				}
				ch = *(format++);
			}

			switch (ch) {
				case 0: {
					return;
				} break;
				case '%': {
					outputc(dest, '%');
				} break;
				case 'u':
				case 'd': {
					itoa(va_arg(va, unsigned int), bf);
					output(dest, bf);
				} break;
				case 'x':
				case 'X': {
					print_hex_common(dest, va_arg(va, uint32_t));
				} break;
				case 'c': {
					outputc(dest, (char)(va_arg(va, int)));
				} break;
				case 's': {
					ptr = va_arg(va, char*);
					output(dest, ptr);
				} break;
				case 'f':
				case 'F': {
					double fnum = va_arg(va, double);
					//TODO find better way to do this
					switch (dest) {
						case TERM_OUTPUT:
							//print integer part, truncate fraction
							printf("%d.", (int)fnum);
							//get numbers after decimal
							fnum = (fnum - (int)fnum) * 1000000;
							printf("%d", (int)fnum);
							break;
						case SERIAL_OUTPUT:
						default:
							//same as above
							printk("%d.", (int)fnum);
							fnum = (fnum - (int)fnum) * 1000000;
							printk("%d", (int)fnum);
							break;
					}
				} break;
				default: {
					terminal_putchar(ch);
				} break;
			}
		}

		//if this is output to kernel log, print process/timestamp info
		//don't go into an infinite loop!
		static bool in_debug_output = false;
		if (!in_debug_output && dest == SERIAL_OUTPUT) {
			//only print debug info if we're on a new line
			if (seen_newline) {
				//mark we're about to use vprintf for debug output
				in_debug_output = true;
				printk_debug_info();
				in_debug_output = false;
				seen_newline = false;
			}
		}
	}
}

void vsprintf(char* ret, char* format, va_list va) {
	char bf[24];
	char ch;

	strcpy(ret, "");

	while ((ch = *(format++)) != 0) {
		if (ch != '%') {
			strccat(ret, ch);
		}
		else {
			// char zero_pad = 0; //TODO: make use of this
			char* ptr;
			// unsigned int len;

			ch = *(format++);

			//zero padding requested
			if (ch == '0') {
				ch = *(format++);
				//TODO fill in
				//if (ch == '\0') ;
				if (ch >= '0' && ch <= '9') {
					// zero_pad = ch - '0';
				}
				ch = *(format++);
			}

			switch (ch) {
				case 0:
					return;

				case 'u':
				case 'd':
					itoa(va_arg(va, unsigned int), bf);
					strcat(ret, bf);
					break;

				case 'x':
				case 'X':
					//printf_hex(va_arg(va, uint32_t));
					//itoa(convert(va_arg(va, unsigned int), 16), bf);
					//terminal_writestring(bf);
					break;

				case 'c':
					strccat(ret, (char)(va_arg(va, int)));
					break;

				case 's':
					ptr = va_arg(va, char*);
					strcat(ret, ptr);
					break;

				case 'f':
				case 'F': {
						char buf[32];
						double fnum = va_arg(va, double);
						//write integer part, truncate fraction
						itoa((int)fnum, (char*)&buf);
						strcat(ret, buf);
						strccat(ret, '.');

						//get numbers after decimal
						fnum = (fnum - (int)fnum) * 100;
						itoa((int)fnum, (char*)&buf);
						strcat(ret, buf);
				} break;

				case '*':
				default:
					strccat(ret, ch);
					break;
			}
		}
	}
}

static lock_t* mutex = 0;
void print_common(int dest, char* format, va_list va) {
	if (dest != TERM_OUTPUT && dest != SERIAL_OUTPUT) {
		ASSERT(0, "print_common called with weird args");
		return;
	}
	//shared printf lock
	if (!mutex) mutex = lock_create();
	lock(mutex);

	vprintf(dest, format, va);

	unlock(mutex);

}

void printf(char* format, ...) {
	//if (!gfx_screen()->vmem) return;
	va_list arg;
	va_start(arg, format);
	print_common(TERM_OUTPUT, format, arg);
	va_end(arg);
}

void printk(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_common(SERIAL_OUTPUT, format, arg);
	va_end(arg);
}

void sprintf(char* buffer, char* format, ...) {
	va_list arg;
	va_start(arg, format);
	vsprintf(buffer, format, arg);
	va_end(arg);
}

enum {
	DBG_PRINT = 0,
	INFO_PRINT,
	ERR_PRINT,
};

void print_msg_common(int dest, int type, char* format, va_list va) {
	switch (type) {
		case DBG_PRINT:
			if (dest == TERM_OUTPUT) {
				printf("\e[10;[\e[11;DEBUG \e[15;");
			}
			else {
				printk("[DEBUG ");
			}
			break;
		case INFO_PRINT:
			if (dest == TERM_OUTPUT) {
				printf("\e[10;[INFO \e[15;");
			}
			else {
				printk("[INFO ");
			}
			break;
		case ERR_PRINT:
		default:
			if (dest == TERM_OUTPUT) {
				printf("\e[10;[\e[12;ERROR \e[15;");
			}
			else {
				printk("[ERROR ");
			}
			break;
	}

	vprintf(dest, format, va);

	if (dest == TERM_OUTPUT) {
		printf("\e[10;]\n");
	}
	else {
		printk("]\n");
	}
}

void printf_dbg(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(TERM_OUTPUT, DBG_PRINT, format, arg);
	va_end(arg);
}
void printf_info(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(TERM_OUTPUT, INFO_PRINT, format, arg);
	va_end(arg);
}
void printf_err(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(TERM_OUTPUT, ERR_PRINT, format, arg);
	va_end(arg);
}

void printk_dbg(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(SERIAL_OUTPUT, DBG_PRINT, format, arg);
	va_end(arg);
}
void printk_info(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(SERIAL_OUTPUT, INFO_PRINT, format, arg);
	va_end(arg);
}
void printk_err(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	print_msg_common(SERIAL_OUTPUT, ERR_PRINT, format, arg);
	va_end(arg);
}

void fprintf(int stream, char* format, ...) {
	va_list arg;
	va_start(arg, format);
	//stdout
	if (stream == 1) {
		print_msg_common(TERM_OUTPUT, INFO_PRINT, format, arg);
	}
	//stderr
	else if (stream == 2) {
		print_msg_common(TERM_OUTPUT, ERR_PRINT, format, arg);
	}
	else {
		ASSERT(0, "fprintf to invalid stream %d\n", stream);
	}
}

