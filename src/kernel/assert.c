#include "assert.h"

void _assert(const char* msg, const char* file, int line) {
	printf("Kernel assertion. %s, line %d: %s\n", file, line, msg);
	asm("cli");
	while (1) {}
}
