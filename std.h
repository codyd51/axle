#if !defined(__cplusplus)
#include <stdbool.h> //C doesn't have boolean type by default
#endif
#include <stddef.h>
#include <stdint.h>

//check if the compiler thinks we're targeting the wrong OS
#if defined(__linux__)
#error "You are not using a cross compiler! You will certainly run into trouble."
#endif

//OS only works for the 32-bit ix86 target
#if !defined(__i386__)
#error "OS must be compiled with a ix86-elf compiler."
#endif



//String functions

//convert integer to string
extern char* itoa(int i, char b[]);
//concatenate strings
char* strcat(char *dest, const char *src);
//concatenate char to string
char* strccat(char* dest, char src);
//compares input strings
int strcmp(const char *lhs, const char *rhs);
//removes last character from string
char* delchar(char* str);
//returns whether input character is alphanumeric
bool isalnum(char ch);
//returns array of strings by delimiting str with delimiter
char* string_split(char* str, char delimiter);
//get length of string
size_t strlen(const char* str);

//Character functions

//returns whether an alphabetical character is uppercase
bool isupper(char ch);
//converts alphabetical character to uppercase equivelent
char toupper(char ch);
//converts alphabetical character to lowercase equivelent
char tolower(char ch);

//Memory functions

//return block of memory of given size
void *malloc(int size);
//free up block of memory
void free(void *ptr);