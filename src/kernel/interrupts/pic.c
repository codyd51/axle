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

    //set interrupt masks here
    //we don't want to mask anything, so the mask is 0x00
    outb(PIC1_DATA, 0x00);
    outb(PIC2_DATA, 0x00);
}

void pic_signal_end_of_interrupt(uint8_t irq_no) {
    //if the interrupt comes from the slave PIC, we need to signal EOI to both the master and slave
    if (irq_no >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
