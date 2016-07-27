#ifndef STD_BASE_H
#define STD_BASE_H

// Check if the compiler thinks we're targeting the wrong OS
#if defined(__linux__)
# error "You are not using a cross compiler! You will certainly run into trouble."
#endif // __linux__

// OS only works for the 32-bit ix86 target
#if !defined(__i386__)
# error "OS must be compiled with a ix86-elf compiler."
#endif // __i386__

// Allow headers to work properly on both C and C++ compilers
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else // __cplusplus
# define __BEGIN_DECLS
# define __END_DECLS
#endif // __cplusplus

// Publicly visible library functions
#ifndef STDAPI
# define STDAPI extern
#endif // STDAPI

#endif // STD_BASE_H
