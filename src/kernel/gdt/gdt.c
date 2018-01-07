#include "gdt.h"
#include <std/memory.h>

#define GDT_SEGMENT_ACCESS_FLAGS_NULL           0x00
#define GDT_SEGMENT_ACCESS_FLAGS_CS             0x9A
#define GDT_SEGMENT_ACCESS_FLAGS_DS             0x92
#define GDT_SEGMENT_ACCESS_FLAGS_USER_CS        0xFA
#define GDT_SEGMENT_ACCESS_FLAGS_USER_DS        0xF2

#define GDT_SEGMENT_GRANULARITY_FLAGS_NULL      0x00
#define GDT_SEGMENT_GRANULARITY_FLAGS_CS        0xCF
#define GDT_SEGMENT_GRANULARITY_FLAGS_DS        0xCF
#define GDT_SEGMENT_GRANULARITY_FLAGS_USER_CS   0xCF
#define GDT_SEGMENT_GRANULARITY_FLAGS_USER_DS   0xCF

//defined in gdt.s
extern void gdt_activate(uint32_t* gdt_pointer);

static void gdt_entry_write_limit_address(gdt_entry_t* ent, uint32_t limit) {
    ent->limit_low  = limit & 0x0FFFF;
    ent->limit_high = limit & 0xF0000;
}

static void gdt_entry_write_base_address(gdt_entry_t* ent, uint32_t base) {
    ent->base_low  = base & 0x0000FFFF;
    ent->base_mid  = base & 0x00FF0000;
    ent->base_high = base & 0xFF000000;
}

static void gdt_entry_write_access_flags(gdt_entry_t* ent, uint8_t flags) {
    memcpy(&ent->access_flags, sizeof(uint8_t), &flags);
}

static void gdt_entry_write_granularity_flags(gdt_entry_t* ent, uint8_t flags) {
    //toss low nibble
    //this flag is only 4 bits
    flags &= 0xF0;
    memcpy(&ent->granularity_flags, sizeof(uint8_t), &flags);
}

static void gdt_setup_null_segment(gdt_entry_t* ent) {
    gdt_entry_write_base_address(ent, 0x00000000);
    gdt_entry_write_limit_address(ent, 0x000FFFFF);

    memset(&ent->access_flags, 0, sizeof(ent->access_flags));
    memset(&ent->granularity_flags, 0, sizeof(ent->granularity_flags));
}

static void gdt_setup_code_segment(gdt_entry_t* ent) {
    //we must write the granularity flags before the limit address because the granularity flags are only 4 bits,
    //but in gdt_entry_write_access_flags we copy a full byte to the struct field where the granularity flags are stored
    //if we'd already written data to the extra 4 bits after the granularity flags, they'd have been overwritten
    gdt_entry_write_access_flags(ent, GDT_SEGMENT_ACCESS_FLAGS_CS);
    gdt_entry_write_granularity_flags(ent, GDT_SEGMENT_GRANULARITY_FLAGS_CS);

    gdt_entry_write_base_address(ent, 0x00000000);
    gdt_entry_write_limit_address(ent, 0x000FFFFF);
}

static void gdt_setup_data_segment(gdt_entry_t* ent) {
    gdt_entry_write_access_flags(ent, GDT_SEGMENT_ACCESS_FLAGS_DS);
    gdt_entry_write_granularity_flags(ent, GDT_SEGMENT_GRANULARITY_FLAGS_DS);

    gdt_entry_write_base_address(ent, 0x00000000);
    gdt_entry_write_limit_address(ent, 0x000FFFFF);
}

static void gdt_setup_user_code_segment(gdt_entry_t* ent) {
    //gdt_entry_write_access_flags(ent, GDT_SEGMENT_ACCESS_FLAGS_USER_CS);
    //gdt_entry_write_granularity_flags(ent, GDT_SEGMENT_GRANULARITY_FLAGS_USER_CS);

    //gdt_entry_write_base_address(ent, 0x00000000);
    gdt_entry_write_limit_address(ent, 0x000FFFFF);
}

static void gdt_setup_user_data_segment(gdt_entry_t* ent) {
    gdt_entry_write_access_flags(ent, GDT_SEGMENT_ACCESS_FLAGS_USER_DS);
    gdt_entry_write_granularity_flags(ent, GDT_SEGMENT_GRANULARITY_FLAGS_USER_DS);

    gdt_entry_write_base_address(ent, 0x00000000);
    gdt_entry_write_limit_address(ent, 0x000FFFFF);
}

void gdt_init() {
    static gdt_entry_t gdt_entries[6]        = {0};
    static gdt_descriptor_t   gdt_descriptor = {0};

    gdt_descriptor.table_base = (uint32_t)&gdt_entries;
    gdt_descriptor.table_size = sizeof(gdt_entries) - 1;

    gdt_setup_null_segment(&gdt_entries[0]);
    gdt_setup_code_segment(&gdt_entries[1]);
    /*
    gdt_setup_data_segment(&gdt_entries[2]);
    gdt_setup_user_code_segment(&gdt_entries[3]);
    gdt_setup_user_data_segment(&gdt_entries[4]);
    */

    //gdt_activate((uint32_t)&gdt_descriptor);
}

