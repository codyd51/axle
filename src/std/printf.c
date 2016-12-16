#include "printf.h"
#include <stdarg.h>
#include <kernel/util/mutex/mutex.h>
#include <kernel/drivers/serial/serial.h>
#include <std/string.h>

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

static void outputc(int dest, char c) {
	switch (dest) {
		case TERM_OUTPUT:
			terminal_putchar(c);
			break;
		case SERIAL_OUTPUT:
		default:
			serial_putchar(c);
			break;
	}
}

static void output(int dest, char* str) {
	while (*str) {
		outputc(dest, *(str++));
	}
}

void print_hex_common(int dest, uint32_t n) {
	unsigned short tmp;
	output(dest, "0x");

	char noZeroes = 1;
	int i;
	for (i = 28; i > 0; i -= 4) {
		tmp = (n >> i) & 0xF;
		if (tmp == 0 && noZeroes != 0) {
			outputc(dest, '0');
			continue;
		}

		if (tmp >= 0xA) {
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

void vprintf(int dest, char* format, va_list va) {
	char bf[24];
	char ch;

	while ((ch = *(format++))) {
		if (ch != '%') {
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
					//labels must be followed by statements, and the declaration below
					//is not a statement, which causes a compiler error
					//to get around this, we have an empty statement
					;

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
			// char zero_pad = 0; //TODO: make use of this
			char* ptr;
			// unsigned int len;

			ch = *(format++);

			//zero padding requested
			if (ch == '0') {
				ch = *(format++);
				if (ch == '\0') return NULL;
				if (ch >= '0' && ch <= '9') {
					// zero_pad = ch - '0';
				}
				ch = *(format++);
			}

			switch (ch) {
				case 0:
					return NULL;

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

				case 'f':
				case 'F': {
						//labels must be followed by statements, and the declaration below
						//is not a statement, which causes a compiler error
						//to get around this, we have an empty statement
						;

						double fnum = va_arg(va, double);
						//print integer part, truncate fraction
						sprintf(ret, "%d.", (int)fnum);
						//get numbers after decimal
						fnum = (fnum - (int)fnum) * 1000000;
						sprintf(ret, "%d", (int)fnum);
					} break;

				case '*':
				default:
					strccat(ret, ch);
					break;
			}
		}
	}
	return ret;
}

void print_common(int dest, char* format, va_list va) {
	//shared printf lock
	static lock_t* mutex = 0;
	//if (!mutex) mutex = lock_create();
	//lock(mutex);

	vprintf(dest, format, va);

	//unlock(mutex);

}

void printf(char* format, ...) {
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

void sprintf(char* str, char* format, ...) {
	va_list arg;
	va_start(arg, format);
	strcat(str, vsprintf(format, arg));
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

