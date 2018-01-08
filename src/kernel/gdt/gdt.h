#ifndef GDT_H
#define GDT_H

#include <stdint.h>

//this structure has a carefully defined format which we must preserve
//see here for format: http://wiki.osdev.org/Global_Descriptor_Table
struct gdt_entry {
    uint32_t low_word;
    uint32_t high_word;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

struct gdt_descriptor {
    uint16_t table_size;
    uint32_t table_base;
} __attribute__((packed));
typedef struct gdt_descriptor gdt_descriptor_t;

void gdt_init(void);

#endif