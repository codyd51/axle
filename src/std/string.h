#ifndef STD_STRING_H
#define STD_STRING_H

#include "std_base.h"
#include <stddef.h>

__BEGIN_DECLS


STDAPI void itoa(int i, char* b);

STDAPI long long int atoi(const char *c);

/// Concatenate strings
STDAPI char* strcat(char *dest, const char *src);

/// Concatenate strings with a given maximum length to copy
STDAPI char* strncat(char *dest, const char *src, int len);

/// Compares input strings
STDAPI int strcmp(const char *lhs, const char *rhs);

/// Removes last character from string
STDAPI char* delchar(char* str);

/// Extract tokens from string
STDAPI char *strtok_r (char *s, const char *delim, char **save_ptr);

/// Returns token at index after delimiting str with delimiter
STDAPI char **strsplit(const char *string, const char *delim, size_t *out);

/// Get length of string
STDAPI size_t strlen(const char* str);

/// Copies src into dest
STDAPI char *strcpy(char *dest, const char *src);

/// Copies src into dest, copying at most @p count bytes
STDAPI char* strncpy(char* dest, const char* src, size_t count);

/// Check if char is blank
STDAPI int isblank(char c);

/// Check if char is space
STDAPI int isspace(char c);

/// Duplicate string and strcpy it
STDAPI char *strdup (const char *s);

/// Search a string for a set of bytes
STDAPI size_t strspn(const char *str, const char *accept);

/// Search a string for a set of bytes
STDAPI size_t srtcspn(const char *str, const char *reject);

/// Search a string for any of a set of bytes
STDAPI char *strpbrk(const char *s, const char *accept);

/// Locate character in string
char *strchr(const char *s, int c_in);

/// Find first occurance of s2 in s1
char *strstr(const char *s1, const char *s2);

__END_DECLS

#endif // STD_STRING_H
