#include "std.h"

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

char* strdelchar(char* str) {
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

size_t strlen(const char* str) {
	size_t ret = 0;
	while (str[ret] != 0) {
		ret++;
	}
	return ret;
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












