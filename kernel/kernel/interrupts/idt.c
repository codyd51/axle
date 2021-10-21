#include "idt.h"
#include "idt_structures.h"
#include "interrupts.h"
#include "pic.h"

#include <kernel/segmentation/gdt_structures.h>
#include <kernel/assert.h>

//extern directives allow us to access the addresses of our ASM ISR handlers

extern void idt_activate(idt_pointer_t* table);

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();
extern void isr16();
extern void isr17();
extern void isr18();
extern void isr19();
extern void isr20();
extern void isr21();
extern void isr22();
extern void isr23();
extern void isr24();
extern void isr25();
extern void isr26();
extern void isr27();
extern void isr28();
extern void isr29();
extern void isr30();
extern void isr31();

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();
extern void irq16();
extern void irq17();
//syscall vector
extern void isr128();
extern void test_isr();

static void idt_set_gate64(idt_descriptor_t* entry, uintptr_t target_addr) {
    idt_descriptor_t desc = {
        .entry_point_low = (target_addr & 0xFFFF),
        .kernel_code_segment_selector = 0x08,
        .ist = 0,
        .type = 0xF, // Comes from the manual, 0b1110
        .always_0 = 0,
        .ring_level = 0,
        .present = 1,
        .entry_point_mid = ((target_addr >> 16) & 0xFFFF),
        .entry_point_high = (target_addr >> 32)
    };
    memcpy(entry, &desc, sizeof(idt_descriptor_t));
}

static void idt_map_all_gates(idt_descriptor_t* idt_entries) {
    // The first 32 interrupt lines will be delivered from the CPU for exceptions
    idt_set_gate64(&idt_entries[ 0], (uintptr_t)isr0);
    idt_set_gate64(&idt_entries[ 1], (uintptr_t)isr1);
    idt_set_gate64(&idt_entries[ 2], (uintptr_t)isr2);
    idt_set_gate64(&idt_entries[ 3], (uintptr_t)isr3);
    idt_set_gate64(&idt_entries[ 4], (uintptr_t)isr4);
    idt_set_gate64(&idt_entries[ 5], (uintptr_t)isr5);
    idt_set_gate64(&idt_entries[ 6], (uintptr_t)isr6);
    idt_set_gate64(&idt_entries[ 7], (uintptr_t)isr7);
    idt_set_gate64(&idt_entries[ 8], (uintptr_t)isr8);
    idt_set_gate64(&idt_entries[ 9], (uintptr_t)isr9);
    idt_set_gate64(&idt_entries[10], (uintptr_t)isr10);
    idt_set_gate64(&idt_entries[11], (uintptr_t)isr11);
    idt_set_gate64(&idt_entries[12], (uintptr_t)isr12);
    idt_set_gate64(&idt_entries[13], (uintptr_t)isr13);
    idt_set_gate64(&idt_entries[14], (uintptr_t)isr14);
    idt_set_gate64(&idt_entries[15], (uintptr_t)isr15);
    idt_set_gate64(&idt_entries[16], (uintptr_t)isr16);
    idt_set_gate64(&idt_entries[17], (uintptr_t)isr17);
    idt_set_gate64(&idt_entries[18], (uintptr_t)isr18);
    idt_set_gate64(&idt_entries[19], (uintptr_t)isr19);
    idt_set_gate64(&idt_entries[20], (uintptr_t)isr20);
    idt_set_gate64(&idt_entries[21], (uintptr_t)isr21);
    idt_set_gate64(&idt_entries[22], (uintptr_t)isr22);
    idt_set_gate64(&idt_entries[23], (uintptr_t)isr23);
    idt_set_gate64(&idt_entries[24], (uintptr_t)isr24);
    idt_set_gate64(&idt_entries[25], (uintptr_t)isr25);
    idt_set_gate64(&idt_entries[26], (uintptr_t)isr26);
    idt_set_gate64(&idt_entries[27], (uintptr_t)isr27);
    idt_set_gate64(&idt_entries[28], (uintptr_t)isr28);
    idt_set_gate64(&idt_entries[29], (uintptr_t)isr29);
    idt_set_gate64(&idt_entries[30], (uintptr_t)isr30);
    idt_set_gate64(&idt_entries[31], (uintptr_t)isr31);

    // Next 16 interrupt lines will be delivered from external devices via the PIC
    idt_set_gate64(&idt_entries[32], (uintptr_t)irq0);
    idt_set_gate64(&idt_entries[33], (uintptr_t)irq1);
    idt_set_gate64(&idt_entries[34], (uintptr_t)irq2);
    idt_set_gate64(&idt_entries[35], (uintptr_t)irq3);
    idt_set_gate64(&idt_entries[36], (uintptr_t)irq4);
    idt_set_gate64(&idt_entries[37], (uintptr_t)irq5);
    idt_set_gate64(&idt_entries[38], (uintptr_t)irq6);
    idt_set_gate64(&idt_entries[39], (uintptr_t)irq7);
    idt_set_gate64(&idt_entries[40], (uintptr_t)irq8);
    idt_set_gate64(&idt_entries[41], (uintptr_t)irq9);
    idt_set_gate64(&idt_entries[42], (uintptr_t)irq10);
    idt_set_gate64(&idt_entries[43], (uintptr_t)irq11);
    idt_set_gate64(&idt_entries[44], (uintptr_t)irq12);
    idt_set_gate64(&idt_entries[45], (uintptr_t)irq13);
    idt_set_gate64(&idt_entries[46], (uintptr_t)irq14);
    idt_set_gate64(&idt_entries[47], (uintptr_t)irq15);

    // Interrupt 128 used as a syscall vector
    idt_set_gate64(&idt_entries[128], (uintptr_t)isr128);
}

void idt_init(void) {
    assert(sizeof(idt_descriptor_t) == 16, "Must be exactly 16 bytes!");

    static idt_descriptor_t idt_entries[256] = {0};
    static idt_pointer_t table = {0};

    table.table_base = (uint32_t)&idt_entries;
    table.table_size = sizeof(idt_entries) - 1;

#define PIC_MASTER_OFFSET	0x20 //int 32 mapped to IRQ 0
#define PIC_SLAVE_OFFSET	0x28 //int 40+ mapped to IRQ8+
    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);

    idt_map_all_gates(idt_entries);
    idt_activate(&table);
}
