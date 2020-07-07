#include "assert.h"
#include <kernel/boot_info.h>

#define _BACKTRACE_SIZE 6

void _panic(const char* msg, const char* file, int line) {
    //enter infinite loop
    asm("cli");
    printf("Assertion failed: %s\n", msg);
    printf("%s:%d\n", file, line);
    if (true) {
        printf("Stack trace:\n");
        uint32_t stack_addrs[_BACKTRACE_SIZE] = {0};
        walk_stack(stack_addrs, _BACKTRACE_SIZE);
        for (int i = 0; i < _BACKTRACE_SIZE; i++) {
            int frame_addr = stack_addrs[i];
            if (!frame_addr) {
                break;
            }
            printf("[%d] %s (0x%08x)\n", i, elf_sym_lookup(&boot_info_get()->kernel_elf_symbol_table, frame_addr), frame_addr);
        }
    }
    asm("cli");
    asm("hlt");
}
