#include "panic.h"
#include "common.h"
#include <stdarg.h>
#include <std/printf.h>

__attribute__((__noreturn__)) void panic(uint16_t line, const char* file) {
	printf("Kernel assertion in %s line %d", file, line);
	//enter infinite loop
    kernel_begin_critical();
	do {} while (1);
}

__attribute__((__noreturn__)) void panic_msg(uint16_t line, const char* file, const char* msg, ...) {
	printf("Kernel assertion in %s line %d: %s\n", file, line, msg);
	//enter infinite loop
    kernel_begin_critical();
	do {} while (1);
}
