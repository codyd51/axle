#include "gdt.h"
#include "gdt_structures.h"
#include <std/memory.h>
#include <stdbool.h>

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

typedef struct gdt_def3 {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    bool accessed:1;
    bool readable:1;
    bool conforming:1;
    bool is_code:1;
    bool belongs_to_os:1;
    uint8_t dpl:2;
    bool present:1;
    uint8_t limit_high:4;
    bool available:1;
    bool long_mode:1;
    bool default_operand_size:1;
    bool granularity:1;
    uint8_t base_high;
} __attribute__((packed)) gdt_def3_t;

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
    uint32_t tmp = limit & 0x000F0000;
    tmp |= (flag);
    printf("tmp 0x%08x\n", tmp);

    //write the high 32 bit word
    //set limit bits 19:16
    entry->high_word  =  limit       & 0x000F0000;
    //set type, p, dpl, s, g, d/b, l and avl fields
    //entry->high_word |= (flag <<  8) & 0x00F0FF00;
    entry->high_word |= flag;
    printf("tm2 0x%08x\n", entry->high_word);
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

typedef struct __attribute__((packed)) {
    uint16_t limit_low; /*!< The low word of the limit */
    uint16_t base_low; /*!< The low word of the base */

    uint8_t base_middle; /*!< The low byte of the upper word of the base */
    uint8_t access; /*!< The access flags for the entry */
    uint8_t limit_high; /*!< The high byte of the limit. Also contains the flags */
    uint8_t base_high; /*!< The high byte of the upper word of the base */
} gdt_entry2_t;

void gdt_set_entry(gdt_entry2_t* entry, uint32_t limit, uint32_t base, uint8_t access, uint8_t flags) {
    // Set up the limit stuff
    entry->limit_low = (uint16_t)(limit & 0xFFFF);
    entry->limit_high = (uint8_t)((limit & 0xF0000) >> 16);

    // Set up the base
    entry->base_low = (uint16_t)(base & 0xFFFF);
    entry->base_middle = (uint8_t)((base & 0xFF0000) >> 16);
    entry->base_high = (uint8_t)((base & 0xFF000000) >> 24);

    // Set up any flags
    entry->limit_high |= (uint8_t)(flags);
    entry->access = access;
}

void gdt_set_flat_entry(gdt_entry2_t* entry, uint8_t access, uint8_t flags) {
    gdt_set_entry(entry, 0xFFFFF, 0, access, flags);
}

void gdt_activate(gdt_pointer_t* table);

void gdt_init() {
    //assert(sizeof(gdt_def3_t) == 8, "Must be exactly 8 bytes!");
    printf("sizeof(gdt_def3_t) = %d\n", sizeof(gdt_def3_t));
    static gdt_entry_t gdt_entries[16] = {0};
    static gdt_pointer_t table = {0};

    table.table_base = (uint32_t)&gdt_entries;
    table.table_size = sizeof(gdt_entries) - 1;

    gdt_write_descriptor(&gdt_entries[0], 0, 0, 0);
    gdt_def3_t cs_kernel_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        .conforming = 0, // todo 0
        .is_code = 1,
        .belongs_to_os = 1,
        .dpl = 0,
        .present = 1,
        .limit_high = 0xF,
        .available = 0, // todo 0
        .long_mode = 1,
        .default_operand_size = 0, // must be 0
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&gdt_entries[1], &cs_kernel_long, sizeof(cs_kernel_long));

    gdt_def3_t ds_kernel_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        .conforming = 0, // todo 0 // For data segment this means expand down
        .is_code = 0,
        .belongs_to_os = 1,
        .dpl = 0,
        .present = 1,
        .limit_high = 0xF,
        .available = 0, // todo 0
        .long_mode = 1,
        .default_operand_size = 0, // must be 0
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&gdt_entries[2], &ds_kernel_long, sizeof(ds_kernel_long));

    /*
Orig 
ES =0030 0000000000000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
CS =0038 0000000000000000 ffffffff 00af9a00 DPL=0 CS64 [-R-]
SS =0030 0000000000000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
DS =0030 0000000000000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
FS =0030 0000000000000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
GS =0030 0000000000000000 ffffffff 00cf9300 DPL=0 DS   [-WA]
LDT=0000 0000000000000000 0000ffff 00008200 DPL=0 LDT
TR =0000 0000000000000000 0000ffff 00008b00 DPL=0 TSS64-busy
GDT=     000000000104a960 0000007f

ES =0010 0000000000000000 ffffffff 00bf9700 DPL=0 DS   [EWA]
CS =0008 0000000000000000 ffffffff 00bf9e00 DPL=0 CS64 [CR-]
SS =0010 0000000000000000 ffffffff 00bf9700 DPL=0 DS   [EWA]
DS =0010 0000000000000000 ffffffff 00bf9700 DPL=0 DS   [EWA]
FS =0010 0000000000000000 ffffffff 00bf9700 DPL=0 DS   [EWA]
GS =0010 0000000000000000 ffffffff 00bf9700 DPL=0 DS   [EWA]
LDT=0000 0000000000000000 0000ffff 00008200 DPL=0 LDT
TR =0000 0000000000000000 0000ffff 00008b00 DPL=0 TSS64-busy
GDT=     000000000104a960 0000007f

ES =0010 0000000000000000 ffffffff 00af9300 DPL=0 DS   [-WA]
CS =0008 0000000000000000 ffffffff 00af9a00 DPL=0 CS64 [-R-]
SS =0010 0000000000000000 ffffffff 00af9300 DPL=0 DS   [-WA]
DS =0010 0000000000000000 ffffffff 00af9300 DPL=0 DS   [-WA]
FS =0010 0000000000000000 ffffffff 00af9300 DPL=0 DS   [-WA]
GS =0010 0000000000000000 ffffffff 00af9300 DPL=0 DS   [-WA]
LDT=0000 0000000000000000 0000ffff 00008200 DPL=0 LDT
TR =0000 0000000000000000 0000ffff 00008b00 DPL=0 TSS64-busy
GDT=     000000000104a960 0000007f
*/
    /*
    gdt_write_descriptor(&gdt_entries[1], 0x00000000, 0x000FFFFF, (GDT_CODE_PL0));
    gdt_write_descriptor(&gdt_entries[2], 0x00000000, 0x000FFFFF, (GDT_DATA_PL0));
    gdt_write_descriptor(&gdt_entries[3], 0x00000000, 0x000FFFFF, (GDT_CODE_PL3));
    gdt_write_descriptor(&gdt_entries[4], 0x00000000, 0x000FFFFF, (GDT_DATA_PL3));
    */

    //gdt_write_descriptor(&gdt_entries[1], 0x0, 0x000FFFFF, (GDT_CODE_R0_x64));
    /*
    uint32_t flags = 0;
    flags |= (1 << 9);      // Readable
    flags |= (1 << 11);     // Always 1
    flags |= (1 << 12);     // 0 = system, 1 = user. Select 1 for code/data segments
    flags |= (0 << 13);     // Ring level (DPL)
    flags |= (1 << 15);     // Present
    flags |= (1 << 21);     // Long mode
    flags |= (0 << 22);     // Default operand size; must be cleared in long mode
    flags |= (1 << 23);     // Granularity: 4KB
    gdt_write_descriptor(&gdt_entries[1], 0x0, 0x000FFFFF, flags);

    flags &= ~(1 << 11);     // Always 0
    flags &= ~(1 << 10);     // Always 0
    flags |= (1 << 9);     // Always 0
    gdt_write_descriptor(&gdt_entries[2], 0x0, 0x000FFFFF, flags);
    */

    //gdt_write_descriptor(&gdt_entries[2], 0x0, 0x0FFFFFFF, (GDT_DATA_R0_x64));
    //tss_init(gdt_entries, 5, 0x10, 0x00);

    printf("Set GDT to %d 0x%x\n", &table, &table);
    gdt_activate(&table);
    //printf("Done load, do jmp\n");
    //gdt_load_cs(SEGMENT_INDEX(1) | SEGMENT_RPL_KERNEL);
    gdt_load_cs(0x08);
    //gdt_load_ds(SEGMENT_INDEX(2) | SEGMENT_RPL_KERNEL);
    gdt_load_ds(0x10);
    asm("cli");
    asm("hlt");
    printf("Done!\n");
    tss_activate();
}

void tss_set_kernel_stack(uint32_t stack) {
   tss_singleton.esp0 = stack;
}

