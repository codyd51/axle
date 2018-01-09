#ifndef IDT_STRUCTURES_H
#define IDT_STRUCTURES_H

#include <stdint.h>

typedef uint8_t idt_entry_flags_t;
//struct describing interrupt gate
struct idt_entry_struct {
   //lower 16 bits of the address to jump to when this interrupt fires
    uint16_t base_lo;
    //kernel segment selector
    uint16_t segment_selector;
    //must always be zero
    uint8_t always0;
    
    //description of this field:
    //bit 0: 0
    //bit 1: 1
    //bot 2: 1
    //bit 3: size of gate (1 == 32 bit gate, 0 == 16 bit gate)
    //bit 4: 0
    //bit 5-6: ring to fire gate in
    //bit 7: present flag
    idt_entry_flags_t flags;

    //upper 16 bits of entry point of interrupt handler
    uint16_t base_hi;
} __attribute__((packed));
typedef struct idt_entry_struct idt_entry_t;

//struct describing pointer to an array of interrupt handlers
//in a format suitable to be passed to 'lidt'
struct idt_descriptor_struct {
    //size (in bytes) of the entire IDT
    uint16_t table_size;
    //address of the first element in idt_entry_t array
    uint32_t table_base;
} __attribute__((packed));
typedef struct idt_descriptor_struct idt_descriptor_t;

typedef struct register_state {
   uint32_t ds;                  // Data segment selector
   uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
   uint32_t int_no, err_code;    // Interrupt number and error code (if applicable)
   uint32_t eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} register_state_t;


#endif