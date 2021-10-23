#include "gdt.h"
#include "gdt_structures.h"
#include <stdbool.h>
#include <std/memory.h>
#include <std/printf.h>
#include <kernel/assert.h>

void gdt_activate(gdt_pointer_t* table);

// TSS definition modified from http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
// TODO(PT): Update TSS gdt entry using manual
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

// Definition only used for TSS code
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

void gdt_init() {
    assert(sizeof(gdt_descriptor_t) == 8, "Must be exactly 8 bytes!");
    static gdt_entry_t gdt_entries[16] = {0};
    static gdt_pointer_t table = {0};

    table.table_base = (uintptr_t)&gdt_entries;
    table.table_size = sizeof(gdt_entries) - 1;

    gdt_descriptor_t null_descriptor = {0};
    memcpy(&gdt_entries[0], &null_descriptor, sizeof(null_descriptor));

    gdt_descriptor_t kernel_code_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-priviliged rings
        .contextual = 0,
        .is_code = 1,
        .belongs_to_os = 1,
        // Ring 0
        .dpl = 0,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&gdt_entries[1], &kernel_code_long, sizeof(kernel_code_long));

    gdt_descriptor_t kernel_data_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the data segment, "Expand down": Whether the meanings of limit and base are flipped
        .contextual = 0,
        .is_code = 0,
        .belongs_to_os = 1,
        // Ring 0
        .dpl = 0,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&gdt_entries[2], &kernel_data_long, sizeof(kernel_data_long));

    /*
    gdt_write_descriptor(&gdt_entries[3], 0x00000000, 0x000FFFFF, (GDT_CODE_PL3));
    gdt_write_descriptor(&gdt_entries[4], 0x00000000, 0x000FFFFF, (GDT_DATA_PL3));
    */
    //tss_init(gdt_entries, 5, 0x10, 0x00);

    gdt_activate(&table);
    gdt_load_cs(0x08);
    gdt_load_ds(0x10);
    printf("Done!\n");
    //tss_activate();
}

void tss_set_kernel_stack(uint32_t stack) {
   tss_singleton.esp0 = stack;
}
