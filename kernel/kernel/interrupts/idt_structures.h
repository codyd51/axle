#ifndef IDT_STRUCTURES_H
#define IDT_STRUCTURES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct idt_descriptor {
    uint16_t entry_point_low;
    uint16_t kernel_code_segment_selector; 
    uint8_t ist:3;
    uint8_t ignored:5;
    uint8_t type:4;
    bool always_0:1;
    uint8_t ring_level:2;
    bool present:1;
    uint16_t entry_point_mid;
    uint32_t entry_point_high;
    uint32_t reserved;
} __attribute__((packed)) idt_descriptor_t;

//struct describing pointer to an array of interrupt handlers
//in a format suitable to be passed to 'lidt'
typedef struct idt_pointer {
    //size (in bytes) of the entire IDT
    uint16_t table_size;
    //address of the first element in idt_entry_t array
    uintptr_t table_base;
} __attribute__((packed)) idt_pointer_t;

typedef struct register_state_i686 {
    uint32_t ds;                  // Data segment selector
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
    uint32_t int_no, err_code;    // Interrupt number and error code (if applicable)
    uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} register_state_i686_t;

typedef struct register_state_x86_64 {
    uint64_t return_ds;
    uint64_t rax, rcx, rdx, rbx, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t int_no, err_code, is_external_interrupt;
    uint64_t return_rip, cs, rflags, return_rsp, ss;
} register_state_x86_64_t;

#if defined __i386__
typedef register_state_i686_t register_state_t;
#elif defined __x86_64__
typedef register_state_x86_64_t register_state_t;
#else
FAIL_TO_COMPILE();
#endif

void registers_print(register_state_t* regs);
idt_pointer_t* kernel_idt_pointer(void);

#endif
