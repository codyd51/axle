#ifndef STD_BASE_H
#define STD_BASE_H

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

#include <stdarg.h>
#include "common.h"

#endif
