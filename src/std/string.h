#ifndef STRING_H
#define STRING_H

#include "std_base.h"

extern char* itoa(int i, char b[]);
//concatenate strings
char* strcat(char *dest, const char *src);
//concatenate char to string
char* strccat(char* dest, char src);
//compares input strings
int strcmp(const char *lhs, const char *rhs);
//removes last character from string
char* delchar(char* str);
//returns token at index after delimiting str with delimiter
char* string_split(char* str, char delimiter, int index);
//get length of string
size_t strlen(const char* str);
//copies src into dest
char *strcpy(char *dest, const char *src);

#endif
