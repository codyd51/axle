#include <stdbool.h>
#include <std/common.h>
#include <kernel/assert.h>
#include "idt.h"
#include "pic.h"

//Some chip-specific PIC defines borrowed from http://wiki.osdev.org/8259_PIC
#define PIC1            0x20 //IO base address for master PIC
#define PIC2            0xA0 //IO base address for slave PIC
#define PIC1_COMMAND    PIC1
#define PIC1_DATA       (PIC1+1)
#define PIC2_COMMAND    PIC2
#define PIC2_DATA       (PIC2+1)

#define ICW1_ICW4		0x01        /* ICW4 (not) needed */
#define ICW1_INIT		0x10        /* Initialization - required! */

#define ICW4_8086		0x01        /* 8086/88 (MCS-80/85) mode */

void pic_remap(int master_pic_int_vector_offset, int slave_pic_int_vector_offset) {
    // Initialize the PICs in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);

    outb(PIC1_DATA, master_pic_int_vector_offset);
    outb(PIC2_DATA, slave_pic_int_vector_offset);
    // Inform the master PIC that there's a slave PIC at IRQ2 (0000 0100)
    outb(PIC1_DATA, 4);
    // Inform the slave PIC of its cascade identity (0000 0010)
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    // Mask all interrupts from the PIC
    // We use the APIC nowadays but set up the PIC to neuter it
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
}

void pic_signal_end_of_interrupt(uint8_t irq_no) {
    // PT: PIT is replaced by the APIC
    Deprecated();
}

void pic_set_interrupt_enabled(int interrupt, bool enabled) {
    // PT: PIT is replaced by the APIC
    Deprecated();
}

bool is_interrupt_vector_delivered_by_pic(int interrupt) {
    return interrupt >= INT_VECTOR_PIC_0 && interrupt <= INT_VECTOR_PIC_15;
}
