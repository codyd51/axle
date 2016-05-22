#include "string.h"
#include <std/kheap.h>

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
	if (index > tokCount) {
		return NULL;
	}

	int iterTokCount = 0;
	i = 0;
	char curTok[50];
	memset(curTok, 0, 50);
	while (str[i] != '\0') {
		if (str[i] == delimiter) {
			curTok[i] = '\0';
			if (iterTokCount == index) {
				char* cpy = kmalloc(sizeof(char) * 64);
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

	char* cpy = kmalloc(sizeof(char) * 64);
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

int isblank(char c) {
	return(c == ' ' || c == '\t');
}

int isspace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\12');
}

char *strdup (const char *s) {
	char *d = kmalloc (strlen (s) + 1);
	if (d == NULL) return NULL;
	strcpy (d,s);
	return d;
}
