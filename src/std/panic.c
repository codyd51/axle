#include "panic.h"
#include "common.h"

void panic(u16int line, const char* file) {
	printf_err("PANIC %s : %d", line, file);
	//enter infinite loop
	do {} while (1);
}

void panic_msg(const char* msg, u16int line, const char* file) {
	printf_err("Throwing panic: %s", msg);
	panic(line, file);
}
