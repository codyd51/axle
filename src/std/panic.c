#include "panic.h"
#include "common.h"
#include <kernel/drivers/terminal/terminal.h>
#include <stdarg.h>
#include <std/std.h>

void pretty_print_frame(void* func) {
	uint32_t addr = (uint32_t)func;
	if (!addr) {
		return; 
	} 
	char* sym = elf_sym_lookup(kern_elf(), addr); 
	printf("%s ", sym); 
	int spaces_needed = 16 - strlen(sym); 
	for (int i = 0; i < spaces_needed; i++) { 
		printf(" "); 
	} 
	printf("@ %x ", addr);
	//hack!
	if (addr < 0x8048080) {
		printf("[KERN]");
	}
	else {
		printf("[ELF ]");
	}
	printf("\n");
}

// Until we have a better way to print stack traces...
#define TRY_PRINT_FRAME(num) do { \
	pretty_print_frame(__builtin_return_address(num)); \
} while(0)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wframe-address"
__attribute__((__always_inline__))
static inline void print_stack(void) {
	printf("Stack trace:\n");

	// First stack frame is a panic() function, so ignore it
	TRY_PRINT_FRAME(1);
	TRY_PRINT_FRAME(2);
	TRY_PRINT_FRAME(3);
	TRY_PRINT_FRAME(4);
	TRY_PRINT_FRAME(5);
	TRY_PRINT_FRAME(6);
	TRY_PRINT_FRAME(7);
	TRY_PRINT_FRAME(8);
	TRY_PRINT_FRAME(9);
	TRY_PRINT_FRAME(10);
	TRY_PRINT_FRAME(11);
	TRY_PRINT_FRAME(12);
	TRY_PRINT_FRAME(13);
	TRY_PRINT_FRAME(14);
	TRY_PRINT_FRAME(15);
	TRY_PRINT_FRAME(16);
}
#pragma GCC diagnostic pop

extern void vprintf(int dest, char* format, va_list va);

__attribute__((__noreturn__)) void panic(uint16_t line, const char* file) {
	printf("\n");
	printf_err("PANIC %s: line %d", file, line);

	print_stack();

	//enter infinite loop
    kernel_begin_critical();
	do {} while (1);
}

__attribute__((__noreturn__)) void panic_msg(uint16_t line, const char* file, const char* msg, ...) {
	//terminal_clear();
	va_list ap;
	va_start(ap, msg);
	//1 == serial output
	vprintf(0, (char*)msg, ap);
	va_end(ap);

	printf_err("\nKernel panic! See syslog for more info.");

	// Inline the panic() code for stack frame count
	printf_err("PANIC %s: line %d", file, line);

	print_stack();

	//enter infinite loop
    kernel_begin_critical();
	do {} while (1);
}
