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
//extract tokens from string
char *strtok_r (char *s, const char *delim, char **save_ptr);
//returns token at index after delimiting str with delimiter
char **strsplit(const char *string, const char *delim, size_t *out);
//get length of string
size_t strlen(const char* str);
//copies src into dest
char *strcpy(char *dest, const char *src);
//check if char is blank
int isblank(char c);
//check if char is space
int isspace(char c);
//duplicate string and strcpy it
char *strdup (const char *s);
//search a string for a set of bytes
size_t strspn(const char *str, const char *accept);
//search a string for a set of bytes
size_t srtcspn(const char *str, const char *reject);
//search a string for any of a set of bytes
char *strpbrk(const char *s, const char *accept);
//locate character in string
char *strchr(const char *s, int c_in);

#endif
