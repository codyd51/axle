#include "assert.h"
#include <kernel/boot_info.h>

#define _BACKTRACE_SIZE 12

void print_stack_trace_old(int frame_count) {
    printf("Stack trace:\n");
    uint32_t stack_addrs[_BACKTRACE_SIZE] = {0};
    walk_stack(stack_addrs, frame_count);
    for (uint32_t i = 0; i < frame_count; i++) {
        int frame_addr = stack_addrs[i];
        if (!frame_addr) {
            break;
        }
        printf("[%d] 0x%08x\n", i, frame_addr);
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
