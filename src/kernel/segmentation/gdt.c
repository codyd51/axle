#include "gdt.h"
#include "gdt_structures.h"
#include <std/memory.h>

// TSS definition modified from http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
typedef struct tss_entry {
	uint32_t prev_tss; 	//previous TSS; would be used if we used hardware task switching
	uint32_t esp0;		//stack pointer to load when changing to kernel mode
	uint32_t ss0;		//stack segment to load when changing to kernel mode
	uint32_t esp1;		//unused...
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint32_t es;		//value to load into ES when changing to kernel mode
	uint32_t cs; 		//as above...
	uint32_t ss;
	uint32_t ds;
	uint32_t fs;
	uint32_t gs;
	uint32_t ldt;		//unused...
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

tss_entry_t tss_singleton = {0};

static void gdt_write_descriptor(gdt_entry_t* entry, uint32_t base, uint32_t limit, uint16_t flag);

typedef struct gdt_def2 {
   uint16_t limit_low;           // The lower 16 bits of the limit.
   uint16_t base_low;            // The lower 16 bits of the base.
   uint8_t  base_middle;         // The next 8 bits of the base.
   uint8_t  access;              // Access flags, determine what ring this segment can be used in.
   uint8_t  granularity;
   uint8_t  base_high;           // The last 8 bits of the base.
} __attribute__((packed)) gdt_def2_t;

static void gdt_set_gate(void* gdt_entries_ptr, int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_def2_t* gdt_entries = gdt_entries_ptr;
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access      = access;
}

static void tss_init(void* gdt_base, uint16_t gdt_offset, uint16_t ss0, uint32_t esp0) {
    // Compute offset into GDT
    // Compute addresses to fill in the GDT
    uint32_t base = (uint32_t)&tss_singleton;
    uint32_t limit = base + sizeof(tss_entry_t);

    // Write it into the GDT
    // Now, add our TSS descriptor's address to the GDT.
    // TODO(PT): Decode these bits and possibly right-shift? (access vs flags)
    gdt_set_gate(gdt_base, gdt_offset, base, limit, 0xE9, 0x00);

    tss_singleton.ss0 = ss0;
    tss_singleton.esp0 = esp0;

    // Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify what
    // segments should be loaded when the processor switches to kernel mode. Therefore
    // they are just our normal kernel code/data segments - 0x08 and 0x10 respectively,
    // but with the last two bits set, making 0x0b and 0x13. The setting of these bits
    // sets the RPL (requested privilege level) to 3, meaning that this TSS can be used
    // to switch to kernel mode from ring 3.
    tss_singleton.cs   = 0x0b;
    tss_singleton.ss = tss_singleton.ds = tss_singleton.es = tss_singleton.fs = tss_singleton.gs = 0x13;
}

static void gdt_write_descriptor(gdt_entry_t* entry, uint32_t base, uint32_t limit, uint16_t flag) {
    //write the high 32 bit word
    //set limit bits 19:16
    entry->high_word  =  limit       & 0x000F0000;
    //set type, p, dpl, s, g, d/b, l and avl fields
    entry->high_word |= (flag <<  8) & 0x00F0FF00;
    //set base bits 23:16
    entry->high_word |= (base >> 16) & 0x000000FF;
    //set base bits 31:24
    entry->high_word |=  base        & 0xFF000000;
 
    //write the low 32 bit word
    //set base bits 15:0
    entry->low_word |= base  << 16;
    //set limit bits 15:0
    entry->low_word |= limit  & 0x0000FFFF;
}

void gdt_init() {
    static gdt_entry_t          gdt_entries[16] = {0};
    static gdt_descriptor_t     gdt_descriptor = {0};

    gdt_descriptor.table_base = (uint32_t)&gdt_entries;
    gdt_descriptor.table_size = sizeof(gdt_entries) - 1;

    gdt_write_descriptor(&gdt_entries[0], 0, 0, 0);
    gdt_write_descriptor(&gdt_entries[1], 0x00000000, 0x000FFFFF, (GDT_CODE_PL0));
    gdt_write_descriptor(&gdt_entries[2], 0x00000000, 0x000FFFFF, (GDT_DATA_PL0));
    gdt_write_descriptor(&gdt_entries[3], 0x00000000, 0x000FFFFF, (GDT_CODE_PL3));
    gdt_write_descriptor(&gdt_entries[4], 0x00000000, 0x000FFFFF, (GDT_DATA_PL3));

    tss_init(gdt_entries, 5, 0x10, 0x00);

    gdt_activate((uint32_t)&gdt_descriptor);
    tss_activate();
}

void tss_set_kernel_stack(uint32_t stack) {
   tss_singleton.esp0 = stack;
}

