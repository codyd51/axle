#ifndef GDT_H
#define GDT_H

#include <stdint.h>

//access flag byte of a GDT segment
struct gdt_segment_access_flag {
    //bit for CPU to use, it gets set to 1 when the segment has been accessed. Should be set to 0
    uint8_t accessed : 1; 
    //if this is a code segment, this flag specifies whether the segment is readable
    //if it's any other segment, the segment is readable and this flag states whether it's writable
    uint8_t rw_permission : 1;
    //if this bit is unset, the base is the lowest address and the limit is the highest (i.e. segment goes from bottom to top)
    //if this bit is set, the base is the highest address and the limit is the lowest (i.e. segment goes from top to bottom)
    uint8_t direction : 1;
    //set is this is the code segment (CS)
    //unset if this is any other segment (DS, SS, ES, FS, GS)
    uint8_t is_code_segment : 1;
    uint8_t always_1 : 1;
    //0-3. specifies ring level for the segment (ring0, ring1, ring2, ring3);
    uint8_t ring_level : 2;
    //always 1 for valid selectors
    uint8_t present_bit : 1;
} __attribute__((packed));
typedef struct gdt_segment_access_flag gdt_segment_access_flag_t;

//granularity flag byte of a GDT segment
struct gdt_segment_granularity_flag {
    //these should just be set to 0
    uint8_t always_0_a : 1;
    uint8_t always_0_b : 1;
    //0 if segment is 16-bit, 1 if segment is 32-bits
    uint8_t segment_size : 1;
    //0 == the segment size (limit - base) should be interpreted as a byte count
    //1 == the segment size (limit - base) should be interpreted as a 4kb-frame count
    uint8_t segment_length : 1;
} __attribute__((packed));
typedef struct gdt_segment_granularity_flag gdt_segment_granularity_flag_t;

//structure contains value of one GDT entry
//use attribute 'packed' to tell GCC not to change
//any of the alignment in the structure
//this structure has a carefully defined format which we must preserve
//see here for format: http://wiki.osdev.org/Global_Descriptor_Table
struct gdt_entry {
    //lower 16 bits of the limit of the segment
    //there is another 4 bits used in this value later in the structure
    uint16_t limit_low;

    //similarly, the lower 16 bits of the address where this segment begins
    //all our segments will start at 0x0
    uint16_t base_low;
    //next 8 bits of base address
    uint8_t base_mid;

    //access flag byte, determining the ring this segment belongs to
    gdt_segment_access_flag_t access_flags;

    //high 4 bits of limit of segment
    uint8_t limit_high : 4;

    //granularity flags specifying how we are addressing the memory in the segment
    gdt_segment_granularity_flag_t granularity_flags;

    //last 8 bits of base address
    uint8_t base_high;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

struct gdt_descriptor {
    uint16_t table_size;
    uint32_t table_base;
} __attribute__((packed));
typedef struct gdt_descriptor gdt_descriptor_t;

void gdt_init(void);

#endif