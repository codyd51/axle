#include "gdt.h"
#include "std/kheap.h"
#include <stdbool.h>
#include <std/memory.h>
#include <std/printf.h>
#include <kernel/assert.h>

void gdt_activate(gdt_pointer_t* table);

static gdt_entry_t _g_gdt_entries[16] = {0};
static gdt_pointer_t _g_gdt_pointer = {0};

// TSS definition modified from http://www.jamesmolloy.co.uk/tutorial_html/10.-User%20Mode.html
// TODO(PT): Update TSS gdt entry using manual
typedef struct tss_entry32 {
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
} __attribute__((packed)) tss_entry32_t;

tss_t tss_singleton = {0};

static void gdt_write_descriptor(gdt_entry_t* entry, uint32_t base, uint32_t limit, uint16_t flag);

// Type: 0xB for busy TSS
// 0x9 for available TSS
typedef struct tss_descriptor_legacy {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t type:4;
    // 'S' bit - 0 for system (LDT, TSS, Gate), 1 for user (Code/Data)
    bool must_be_zero:1;
    uint8_t dpl:2;
    bool present:1;
    uint8_t limit_high:4;
    bool unused_bit:1;
    bool ignored:1;
    bool default_operand_size:1;
    bool granularity:1;
    uint8_t base_high;
} __attribute__((packed)) tss_descriptor_legacy_t;

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

tss_t* tss_descriptor_create_for_long_mode(tss_descriptor_t* out) {
    tss_t* tss = calloc(1, sizeof(tss_t));
    uintptr_t base = (uintptr_t)tss;
    uintptr_t limit = base + sizeof(tss_t);
    tss_descriptor_t tss_descriptor = {
        .limit_low = (limit >> 0) & 0xFFFF,
        .base_low = (base >> 0) & 0xFFFF,
        .base_lower_middle = (base >> 16) & 0xFF,
        .type = 0x9,
        .must_be_zero = 0,
        .dpl = 3,
        .present = 1,
        .limit_high = (limit >> 16) & 0xF,
        .unused_bit = 0,
        .ignored = 0,
        .granularity = 1,
        .base_upper_middle = (base >> 24) & 0xFF,
        .base_high = (base >> 32) & 0xFFFFFFFF,
        .must_be_zero_high = 0,
    };
    memcpy(out, &tss_descriptor, sizeof(tss_descriptor_t));
    return tss;
}

static void tss_init(gdt_descriptor_t* gdt) {
    assert(sizeof(tss_descriptor_t) == 16, "TSS descriptor must be exactly 16 bytes!");

    assert(sizeof(tss_t) == 104, "TSS must be exactly 104 bytes!");

    memset(&tss_singleton, 0, sizeof(tss_singleton));

    uintptr_t base = &tss_singleton;
    uintptr_t limit = base + sizeof(tss_t);

    tss_descriptor_t tss_descriptor = {
        .limit_low = (limit >> 0) & 0xFFFF,
        .base_low = (base >> 0) & 0xFFFF,
        .base_lower_middle = (base >> 16) & 0xFF,
        .type = 0x9,
        .must_be_zero = 0,
        .dpl = 3,
        .present = 1,
        .limit_high = (limit >> 16) & 0xF,
        .unused_bit = 0,
        .ignored = 0,
        .granularity = 1,
        .base_upper_middle = (base >> 24) & 0xFF,
        .base_high = (base >> 32) & 0xFFFFFFFF,
        .must_be_zero_high = 0,
    };
    memcpy(&gdt[5], &tss_descriptor, sizeof(tss_descriptor));
}

gdt_descriptor_t* gdt_create_for_protected_mode(uintptr_t* out_size) {
    *out_size = sizeof(gdt_descriptor_t) * 16;
    gdt_descriptor_t* table = kmalloc(*out_size);

    // Null entry
    memset(&table[0], 0, sizeof(gdt_descriptor_t));
    // Kernel code
    table[1] = (gdt_descriptor_t){
        .limit_low = 0xffff,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-privileged rings
        .contextual = 0,
        // These 2 fields should always be 1 according to the AMD manual
        .is_code = 1,
        .belongs_to_os = 1,
        .dpl = 0,
        .present = 1,
        .limit_high = 0xf,
        .unused_bit = 0,
        .long_mode = 0,
        // 0 = 16bit operands, 1 = 32bit operands
        .default_operand_size = 1,
        // 0 = segment limit is a literal, 1 = segment limit is a multiple of 4k pages
        .granularity = 1,
        .base_high = 0x0,
    };
    // Kernel data
    table[2] = (gdt_descriptor_t){
        .limit_low = 0xffff,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        // In a data segment, this bit represents whether the data is writable
        .readable = 1,
        // In the data segment, "Expand-down": inverts the meanings of limit and base
        .contextual = 0,
        // These 2 fields should be 0 and 1 according to the AMD manual
        .is_code = 0,
        .belongs_to_os = 1,
        .dpl = 0,
        .present = 1,
        .limit_high = 0xf,
        .unused_bit = 0,
        .long_mode = 0,
        // 0 = 16bit operands, 1 = 32bit operands
        .default_operand_size = 1,
        // 0 = segment limit is a literal, 1 = segment limit is a multiple of 4k pages
        .granularity = 1,
        .base_high = 0x0,
    };

    return table;
}

void gdt_create_for_long_mode(gdt_descriptor_t** gdt_out, uintptr_t* gdt_out_size, tss_t** tss_out) {
    *gdt_out_size = sizeof(gdt_descriptor_t) * 16;
    gdt_descriptor_t* table = kcalloc(16, sizeof(gdt_descriptor_t));
    *gdt_out = table;

    gdt_descriptor_t null_descriptor = {0};
    memcpy(&table[0], &null_descriptor, sizeof(null_descriptor));

    gdt_descriptor_t kernel_code_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-privileged rings
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
    memcpy(&table[1], &kernel_code_long, sizeof(kernel_code_long));

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
    memcpy(&table[2], &kernel_data_long, sizeof(kernel_data_long));

    gdt_descriptor_t user_code_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-priviliged rings
        .contextual = 0,
        .is_code = 1,
        .belongs_to_os = 1,
        // Ring 3
        .dpl = 3,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&table[3], &user_code_long, sizeof(user_code_long));

    gdt_descriptor_t user_data_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the data segment, "Expand down": Whether the meanings of limit and base are flipped
        .contextual = 0,
        .is_code = 0,
        .belongs_to_os = 1,
        // Ring 3
        .dpl = 3,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&table[4], &user_data_long, sizeof(user_data_long));

    tss_descriptor_t tss_descriptor = {0};
    *tss_out = tss_descriptor_create_for_long_mode(&tss_descriptor);
    memcpy(&table[5], &tss_descriptor, sizeof(tss_descriptor_t));
}

void gdt_init() {
    assert(sizeof(gdt_descriptor_t) == 8, "GDT descriptor must be exactly 8 bytes!");
    assert(sizeof(tss_descriptor_t) == 16, "TSS descriptor must be exactly 16 bytes!");

    _g_gdt_pointer.table_base = (uintptr_t)&_g_gdt_entries;
    _g_gdt_pointer.table_size = sizeof(_g_gdt_entries) - 1;

    gdt_descriptor_t null_descriptor = {0};
    memcpy(&_g_gdt_entries[0], &null_descriptor, sizeof(null_descriptor));

    gdt_descriptor_t kernel_code_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-privileged rings
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
    memcpy(&_g_gdt_entries[1], &kernel_code_long, sizeof(kernel_code_long));

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
    memcpy(&_g_gdt_entries[2], &kernel_data_long, sizeof(kernel_data_long));

    gdt_descriptor_t user_code_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the code segment, "Conforming": Whether code in this segment can be run in less-priviliged rings
        .contextual = 0,
        .is_code = 1,
        .belongs_to_os = 1,
        // Ring 3
        .dpl = 3,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&_g_gdt_entries[3], &user_code_long, sizeof(user_code_long));

    gdt_descriptor_t user_data_long = {
        .limit_low = 0xFFFF,
        .base_low = 0x0,
        .base_middle = 0x0,
        .accessed = 0,
        .readable = 1,
        // In the data segment, "Expand down": Whether the meanings of limit and base are flipped
        .contextual = 0,
        .is_code = 0,
        .belongs_to_os = 1,
        // Ring 3
        .dpl = 3,
        .present = 1,
        .limit_high = 0xF,
        .unused_bit = 0,
        .long_mode = 1,
        // Must be 0 in long mode
        .default_operand_size = 0,
        .granularity = 1,
        .base_high = 0
    };
    memcpy(&_g_gdt_entries[4], &user_data_long, sizeof(user_data_long));

    gdt_activate(&_g_gdt_pointer);
    gdt_load_cs(0x08);
    gdt_load_ds(0x10);
    tss_init(&_g_gdt_entries);
    tss_activate();
}

void tss_set_kernel_stack(uint64_t stack) {
   tss_singleton.rsp0_low = (stack & 0xFFFFFFFF);
   tss_singleton.rsp0_high = ((stack >> 32) & 0xFFFFFFFF);
}

gdt_pointer_t* kernel_gdt_pointer(void) {
    return &_g_gdt_pointer;
}
