#include "assert.h"
#include <kernel/boot_info.h>

#define _BACKTRACE_SIZE 12

void print_stack_trace(int frame_count) {
    printf("Stack trace:\n");
    uint32_t stack_addrs[_BACKTRACE_SIZE] = {0};
    walk_stack(stack_addrs, frame_count);
    for (uint32_t i = 0; i < frame_count; i++) {
        int frame_addr = stack_addrs[i];
        if (!frame_addr) {
            break;
        }
        const char* sym_name = elf_sym_lookup(&boot_info_get()->kernel_elf_symbol_table, frame_addr);
        // Skip unprintable symbol names 
        // This might be an address without a symbolicated entry point
        if (!sym_name) {
            continue;
        }
        bool is_ascii = true;
        for (int i = 0; i < strlen(sym_name); i++) {
            if (sym_name[i] < '?' || sym_name[i] > 'z') {
                is_ascii = false;
                break;
            }
        }
        if (is_ascii) {
            printf("[%d] %s (0x%08x)\n", i, sym_name, frame_addr);
        }
    }
}

void _panic(const char* msg, const char* file, int line) {
    //enter infinite loop
    asm("cli");
    printf("[%d] Assertion failed: %s\n", getpid(), msg);
    printf("%s:%d\n", file, line);
    if (true) {
        print_stack_trace(12);
    }
    asm("cli");
    asm("hlt");
}
