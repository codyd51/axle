#include "idt.h"
#include "idt_structures.h"
#include "isr.h"
#include "pic.h"

#include <kernel/gdt/gdt_structures.h>
#include <kernel/assert.h>

static void idt_set_gate(idt_entry_t* entry, uint32_t base, uint16_t sel, idt_entry_flags_t flags) {
    /*
    base:
        The function pointer to be executed when the interrupt is received
    sel:
        Privilege level of the function
    flags:
        x86 flags byte. See idt_structures.h for definition
    */
    entry->base_lo              = base & 0xFFFF;
    entry->base_hi              = (base >> 16) & 0xFFFF;
    entry->segment_selector     = sel;
    entry->always0              = 0;
    //we must uncomment the OR below when we get to user mode
    //it sets the interrupt gate's privilege level to 3
    entry->flags                = flags/* | 0x60*/;
}

static void idt_map_all_gates(idt_entry_t* table) {
    //the first 32 interrupt lines will be delivered from the CPU
    idt_set_gate(&table[ 0], (uint32_t)isr0 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 1], (uint32_t)isr1 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 2], (uint32_t)isr2 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 3], (uint32_t)isr3 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 4], (uint32_t)isr4 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 5], (uint32_t)isr5 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 6], (uint32_t)isr6 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 7], (uint32_t)isr7 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 8], (uint32_t)isr8 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[ 9], (uint32_t)isr9 , GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[10], (uint32_t)isr10, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[11], (uint32_t)isr11, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[12], (uint32_t)isr12, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[13], (uint32_t)isr13, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[14], (uint32_t)isr14, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[15], (uint32_t)isr15, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[16], (uint32_t)isr16, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[17], (uint32_t)isr17, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[18], (uint32_t)isr18, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[19], (uint32_t)isr19, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[20], (uint32_t)isr20, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[21], (uint32_t)isr21, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[22], (uint32_t)isr22, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[23], (uint32_t)isr23, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[24], (uint32_t)isr24, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[25], (uint32_t)isr25, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[26], (uint32_t)isr26, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[27], (uint32_t)isr27, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[28], (uint32_t)isr28, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[29], (uint32_t)isr29, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[30], (uint32_t)isr30, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);
    idt_set_gate(&table[31], (uint32_t)isr31, GDT_BYTE_INDEX_KERNEL_CODE, 0x08E);

    /*
    //interrupts above 32 will be delivered by the OS
    idt_set_gate(&table[32], (uint32_t)irq0 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[33], (uint32_t)irq1 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[34], (uint32_t)irq2 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[35], (uint32_t)irq3 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[36], (uint32_t)irq4 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[37], (uint32_t)irq5 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[38], (uint32_t)irq6 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[39], (uint32_t)irq7 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[40], (uint32_t)irq8 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[41], (uint32_t)irq9 , GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[42], (uint32_t)irq10, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[43], (uint32_t)irq11, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[44], (uint32_t)irq12, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[45], (uint32_t)irq13, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[46], (uint32_t)irq14, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[47], (uint32_t)irq15, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    idt_set_gate(&table[128], (uint32_t)isr128, GDT_BYTE_INDEX_KERNEL_CODE, 0x8E);
    */
}

void idt_init(void) {
    static idt_entry_t idt_entries[256] = {0};
    static idt_descriptor_t idt_ptr;

    idt_ptr.table_base = (uint32_t)&idt_entries;
    idt_ptr.table_size = sizeof(idt_entries) - 1;

#define PIC_MASTER_OFFSET	0x28
#define PIC_SLAVE_OFFSET	0x28
	pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);

    idt_map_all_gates(idt_entries);
    idt_activate((uint32_t)&idt_ptr);
}
