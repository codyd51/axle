#include "gdt.h"
#include "gdt_structures.h"
#include <std/memory.h>
 
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
    static gdt_entry_t          gdt_entries[5] = {0};
    static gdt_descriptor_t     gdt_descriptor = {0};

    gdt_descriptor.table_base = (uint32_t)&gdt_entries;
    gdt_descriptor.table_size = sizeof(gdt_entries) - 1;

    gdt_write_descriptor(&gdt_entries[0], 0, 0, 0);
    gdt_write_descriptor(&gdt_entries[1], 0x00000000, 0x000FFFFF, (GDT_CODE_PL0));
    gdt_write_descriptor(&gdt_entries[2], 0x00000000, 0x000FFFFF, (GDT_DATA_PL0));
    gdt_write_descriptor(&gdt_entries[3], 0x00000000, 0x000FFFFF, (GDT_CODE_PL3));
    gdt_write_descriptor(&gdt_entries[4], 0x00000000, 0x000FFFFF, (GDT_DATA_PL3));

    gdt_activate((uint32_t)&gdt_descriptor);
}
