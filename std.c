#include "std.h"
#include <stdarg.h>

//String functions

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

char* strcat(char *dest, const char *src) {
	size_t i,j;
	for (i = 0; dest[i] != '\0'; i++)
			;
	for (j = 0; src[j] != '\0'; j++)
			dest[i+j] = src[j];
	dest[i+j] = '\0';
	return dest;
}

char* strccat(char* dest, char src) {
	size_t i;
	for (i = 0; dest[i] != '\0'; i++)
		;
	dest[i] = src;
	dest[i+1] = '\0';
	return dest;
}

char* delchar(char* str) {
	size_t i;
	for (i = 0; str[i] != '\0'; i++)
		;
	if (i >= 1) {
		str[i-1] = '\0';
		return str;
	}
	return "";
}

int strcmp(const char *lhs, const char *rhs) {
	if (strlen(lhs) != strlen(rhs)) {
		return -1;
	}

	size_t ch = 0;
	size_t ch2 = 0;

	while (lhs[ch] != 0 && rhs[ch2] != 0) {
		if (lhs[ch] != rhs[ch2]) {
			return -1;
		}
		ch++;
		ch2++;
	}
	return 0;
}

char* string_split(char* str, char delimiter, int index) {
	int i = 0;
	int tokCount = 0;
	while (str[i] != '\0') {
		if (str[i] == delimiter) {
			tokCount++;
		}
		i++;
	}
	if (index > tokCount) return "";

	int iterTokCount = 0;
	i = 0;
	char curTok[50] = "";
	while (str[i] != '\0') {
		if (str[i] == delimiter) {
			curTok[i] = '\0';
			if (iterTokCount == index) {
				char* cpy = malloc(sizeof(char) * 64);
				strcpy(cpy, curTok);

				return cpy;
			}
			else {
				iterTokCount++;
				for (int i = 0; i < 50; i++) {
					curTok[i] = '\0';
				}
			}
		}
		else {
			strccat(curTok, str[i]);
		}
		i++;
	}

	char* cpy = malloc(sizeof(char) * 64);
	strcpy(cpy, curTok);

	return cpy;
}

size_t strlen(const char* str) {
	size_t ret = 0;
	while (str[ret] != 0) {
		ret++;
	}
	return ret;
}
#import "kernel.h"
char *strcpy(char *dest, const char *src) {
	int i = 0;
	while (1) {
		dest[i] = src[i];
		if (dest[i] == '\0') {
			break;
		}
		i++;
	}
	return dest;
}

//Character functions

bool isalpha(char ch) {
	char* az = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (int i = 0; i < strlen(az); i++) {
		if (ch == az[i]) return true;
	}
	return false;
}

bool isalnum(char ch) {
	char* nums = "0123456789";
	for (int i = 0; i < strlen(nums); i++) {
		if (ch == nums[i]) return true;
	}
	return isalpha(ch);
}

bool isupper(char ch) {
	char* up = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (int i = 0; i < strlen(up); i++) {
		if (ch == up[i]) return true;
	}
	return false;
}
char toupper(char ch) {
	//if already uppercase or not an alphabetic, just return the character
	if (isupper(ch) || !isalpha(ch)) return ch;

	return ch - 32;
}

char tolower(char ch) {
	//if already lowercase or not an alphabetic, just return the character
	if (!isupper(ch) || !isalpha(ch)) return ch;

	return ch + 32;
}

//Memory functions
static char memory_data[32768];
static char *mem_end;

void initmem(void) {
  mem_end = memory_data;
}

void *malloc(int size) {
  char *temp = mem_end;
  mem_end += size;
  return (void*) temp;
}

void free(void *ptr) {
  /* Don't bother to free anything--if programs need to start over, they
     can re-invoke initmem */
}

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i]) {
			return -1;
		}
		else if (b[i] > a[i]) {
			return 1;
		}
	}
	return 0;
}

void* memset(void* bufptr, int value, size_t size) {
	unsigned char* buf = (unsigned char*)bufptr;
	for (size_t i = 0; i < size; i++) {
		buf[i] = (unsigned char)value;
	}
}

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
	unsigned char* dst = (unsigned char*)dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	for (size_t i = 0;  i < size; i++) {
		dst[i] = src[i];
	}
	return dstptr;
}

//Printing functions

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
					itoa(convert(va_arg(va, unsigned int), 16), bf);
					terminal_writestring(bf);
					break;

				case 'c':
					terminal_putchar((char)(va_arg(va, int)));
					break;

				case 's':
					ptr = va_arg(va, char*);
					terminal_writestring(ptr);
					break;

				default:
					terminal_putchar(ch);
					break;
			}
		}
	}
}
void printf(char* format, ...) {
	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);
}

void printf_dbg(char* format, ...) {
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_MAGENTA);
	printf("DEBUG ");
	terminal_settextcolor(COLOR_LIGHT_BLUE);

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
	terminal_settextcolor(COLOR_LIGHT_BLUE);

	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);

	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("]\n");	
}

void printf_err(char* format, ...) {
	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("[");
	terminal_settextcolor(COLOR_LIGHT_RED);
	printf("ERROR ");
	terminal_settextcolor(COLOR_LIGHT_BLUE);

	va_list arg;
	va_start(arg, format);
	vprintf(format, arg);
	va_end(arg);

	terminal_settextcolor(COLOR_LIGHT_GREEN);
	printf("]\n");	
}












