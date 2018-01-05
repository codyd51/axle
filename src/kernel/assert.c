#include "assert.h"

void _panic(const char* msg, const char* file, int line) {
    printf("Kernel assertion in %s line %d", file, line);
    //enter infinite loop
    asm("cli");
    do {} while (1);
}
