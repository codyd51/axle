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
void printf(char* format, ...) {
	char* traverse;
	unsigned int i;
	char* s;

	//initializing printf's arguments
	va_list arg;
	va_start(arg, format);

	int counter = 0;
	for (traverse = format; counter < strlen(format); traverse++) {
		counter++;

		while (*traverse != '%' && counter < strlen(format)) {
			terminal_putchar(*traverse);
			traverse++;
			counter++;
		}

		traverse++;

		//fetching & ececuting arguments
		switch (*traverse) {
			case 'c':
				//fetch char argument
				i = va_arg(arg, int);
				terminal_putchar(i);
				break;
			case 'd':
			case 'i':
				//fetch decimal/int argument
				i = va_arg(arg, int);
				if (i < 0) {
					i = -i;
					terminal_putchar('-');
				}
				terminal_writestring(convert(i, 10));
				break;
			case 'o':
				//octal argument
				i = va_arg(arg, unsigned int);
				terminal_writestring(convert(i, 8));
				break;
			case 's':
				//fetch string
				s = va_arg(arg, char*);
				terminal_writestring(s);
				break;
			case 'x':
				//fetch hex
				i = va_arg(arg, unsigned int);
				terminal_writestring(convert(i, 16));
				break;
			/*
			default:
				//unknown arg type
				//interpret as int
				i = va_arg(arg, unsigned int);
				terminal_writestring(convert(i, 10));
				break;
			*/
		}
	}

	//cleanup
	va_end(arg);
}












