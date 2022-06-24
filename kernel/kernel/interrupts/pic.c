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

#define PIC_EOI			0x20		/* End-of-interrupt command */

#define ICW1_ICW4		0x01        /* ICW4 (not) needed */
#define ICW1_SINGLE		0x02        /* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04        /* Call address interval 4 (8) */
#define ICW1_LEVEL		0x08        /* Level triggered (edge) mode */
#define ICW1_INIT		0x10        /* Initialization - required! */

#define ICW4_8086		0x01        /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO		0x02        /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08        /* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C        /* Buffered mode/master */
#define ICW4_SFNM		0x10        /* Special fully nested (not) */

void pic_remap(int offset1, int offset2) {
    /*
     * offset1:
     *		vector offset for master PIC
     * offset2:
     *		vector offset for slave PIC
     */
    //starts PIC initialization process
    //in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);

    //master PIC 
    outb(PIC1_DATA, offset1);                 // ICW2: Master PIC vector offset
    outb(PIC2_DATA, offset2);                 // ICW2: Slave PIC vector offset
    outb(PIC1_DATA, 4);                       // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    // Mask all interrupts by default
    // They'll be unmasked one-by-one as we register interrupt handlers
    // But be sure to allow IRQ2 as it's how the slave PIC will talk to the master
    outb(PIC1_DATA, 0xff);
    outb(PIC2_DATA, 0xff);
    pic_set_interrupt_enabled(INT_VECTOR_IRQ2, true);
}

void pic_signal_end_of_interrupt(uint8_t irq_no) {
    //if the interrupt comes from the slave PIC, we need to signal EOI to both the master and slave
    if (irq_no >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_set_interrupt_enabled(int interrupt, bool enabled) {
    assert(is_interrupt_vector_delivered_by_pic(interrupt), "Invalid interrupt vector");

    int irq_no = interrupt - INT_VECTOR_IRQ0;
    int bit = irq_no;
    int desired_pic = PIC1_DATA;

    if (irq_no >= 8) {
        bit = irq_no - 8;
        desired_pic = PIC2_DATA;
    }

    // Read the current interrupt mask, then set our desired bit
    int current_int_mask = inb(desired_pic);
    if (enabled) {
        current_int_mask &= ~(1 << bit);
    }
    else {
        current_int_mask |= (1 << bit);
    }
    // Write it back
    outb(desired_pic, current_int_mask);
}

bool is_interrupt_vector_delivered_by_pic(int interrupt) {
    return interrupt >= INT_VECTOR_IRQ0 && interrupt <= INT_VECTOR_IRQ15;
}
