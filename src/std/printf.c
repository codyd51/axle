#include "printf.h"
#include <stdarg.h>

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
void printf_hex(uint32_t n) {
	unsigned short tmp;
	terminal_writestring("0x");

	char noZeroes = 1;
	int i;
	for (i = 28; i > 0; i -= 4) {
		tmp = (n >> i) & 0xF;
		if (tmp == 0 && noZeroes != 0) {
			printf("0");
			continue;
		}

		if (tmp >= 0xA) {
			noZeroes = 0;
			terminal_putchar(tmp-0xA + 'a');
		} 
		else {
			noZeroes = 0;
			terminal_putchar(tmp + '0');
		}
	}


	tmp = n & 0xF;
	if (tmp >= 0xA) {
		terminal_putchar(tmp-0xA + 'a');
	} 
	else {
		terminal_putchar(tmp + '0');
	}
}

void vprintf(char* format, va_list va) {
	char bf[24];
	char ch;

	while ((ch = *(format++))) {
		if (ch != '%') {
			terminal_putchar(ch);
		}
		else {
			char zero_pad = 0;
			char* ptr;
			unsigned int len;

			ch = *(format++);

			//zero padding requested
			if (ch == '0') {
				ch = *(format++);
				if (ch == '\0') return;
				if (ch >= '0' && ch <= '9') {
					zero_pad = ch - '0';
				}
				ch = *(format++);
			}

			switch (ch) {
				case 0:
					return;

				case 'u':
				case 'd':
					itoa(va_arg(va, unsigned int), bf);
					terminal_writestring(bf);
					break;

				case 'x':
				case 'X':
					printf_hex(va_arg(va, uint32_t));
					break;

				case 'c':
					terminal_putchar((char)(va_arg(va, int)));
					break;

				case 's':
					ptr = va_arg(va, char*);
					terminal_writestring(ptr);
					break;

				case 'f':
				case 'F':
					//labels must be followed by statements, and the declaration below
					//is not a statement, which causes a compiler error
					//to get around this, we have an empty statement
					;

					double fnum = va_arg(va, double);
					//print integer part, truncate fraction
					printf("%d.", (int)fnum);
					//get numbers after decimal
					fnum = (fnum - (int)fnum) * 1000000;
					printf("%d", (int)fnum);
					break;

				case '%':
				default:
					terminal_putchar(ch);
					break;
			}
		}
	}
}

char* vsprintf(char* format, va_list va) {
	char bf[24];
	char ch;

	char* ret = "";

	while ((ch = *(format++)) != 0) {
		if (ch != '%') {
			strccat(ret, ch);	
		}
		else {
			char zero_pad = 0;
			char* ptr;
			unsigned int len;

			ch = *(format++);

			//zero padding requested
			if (ch == '0') {
				ch = *(format++);
				if (ch == '\0') return;
				if (ch >= '0' && ch <= '9') {
					zero_pad = ch - '0';
				}
				ch = *(format++);
			}

			switch (ch) {
				case 0:
					return;

				case 'u':
				case 'd':
					itoa(va_arg(va, unsigned int), bf);
					strcat(format, bf);
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
				case '*':

				default:
					strccat(ret, ch);
					break;
			}
		}
	}
	return ret;
}


void printf(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);
}

void sprintf(char* str, char* format, ...) {
	printf_dbg("format: %s", format);
	va_list arg;
	va_start(arg, format);
	strcat(str, vsprintf(format, arg));
	va_end(arg);
}

void printf_dbg(char* format, ...) {
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_MAGENTA);
	printf("DEBUG ");
	terminal_settextcolor(COLOR_WHITE);

	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);

	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("]\n");	
}

void printf_info(char* format, ...) {
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("INFO ");
	terminal_settextcolor(COLOR_WHITE);

	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);

	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("]\n");	
}

void printf_err(const char* format, ...) {
	va_list ap;
	va_start(ap, format);
	vprintf_err(format, ap);
	va_end(ap);
}

void vprintf_err(const char* format, va_list ap) {
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_RED);
	printf("ERROR ");
	terminal_settextcolor(COLOR_WHITE);

	vprintf(format, ap);

	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("]\n");
}
