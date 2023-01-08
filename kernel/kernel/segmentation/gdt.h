#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stdbool.h>

// Ref: AMD manual Vol 2, section 4.7.2
typedef struct gdt_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    bool accessed:1;
    bool readable:1;
    bool contextual:1;
    bool is_code:1;
    bool belongs_to_os:1;
    uint8_t dpl:2;
    bool present:1;
    uint8_t limit_high:4;
    // This bit is available for OS use
    bool unused_bit:1;
    bool long_mode:1;
    bool default_operand_size:1;
    bool granularity:1;
    uint8_t base_high;
} __attribute__((packed)) gdt_descriptor_t;

void gdt_init(void);
// Must be performed upon task switch to allow the kernel to be preemptible
void tss_set_kernel_stack(uint64_t stack);
gdt_descriptor_t* gdt_create_for_protected_mode(uintptr_t* out_size);

#endif