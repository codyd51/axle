#ifndef GDT_H
#define GDT_H

#include <stdint.h>
#include <stdbool.h>

#include "gdt_structures.h"

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

typedef struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_lower_middle;
    uint8_t type:4;
    // 'S' bit - 0 for system (LDT, TSS, Gate), 1 for user (Code/Data)
    bool must_be_zero:1;
    uint8_t dpl:2;
    bool present:1;
    uint8_t limit_high:4;
    bool unused_bit:1;
    uint8_t ignored:2;
    bool granularity:1;
    uint8_t base_upper_middle;
    uint32_t base_high;
    uint8_t reserved;
    uint8_t must_be_zero_high:5;
    uint32_t reserved_high:19;
} __attribute__((packed)) tss_descriptor_t;

typedef struct tss {
    uint32_t reserved1;

    uint32_t rsp0_low;
    uint32_t rsp0_high;
    uint32_t rsp1_low;
    uint32_t rsp1_high;
    uint32_t rsp2_low;
    uint32_t rsp2_high;

    uint64_t reserved2;

    uint32_t ist1_low;
    uint32_t ist1_high;
    uint32_t ist2_low;
    uint32_t ist2_high;
    uint32_t ist3_low;
    uint32_t ist3_high;
    uint32_t ist4_low;
    uint32_t ist4_high;
    uint32_t ist5_low;
    uint32_t ist5_high;
    uint32_t ist6_low;
    uint32_t ist6_high;
    uint32_t ist7_low;
    uint32_t ist7_high;

    uint64_t reserved3;

    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

void gdt_init(void);
// Must be performed upon task switch to allow the kernel to be preemptible
void tss_set_kernel_stack(uint64_t stack);
gdt_descriptor_t* gdt_create_for_protected_mode(uintptr_t* out_size);
void gdt_create_for_long_mode(gdt_descriptor_t** gdt_out, uintptr_t* gdt_out_size, tss_t** tss_out);

gdt_pointer_t* kernel_gdt_pointer(void);

tss_t* tss_descriptor_create_for_long_mode(tss_descriptor_t* out);

#endif