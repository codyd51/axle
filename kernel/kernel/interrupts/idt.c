#include "idt.h"
#include "idt_structures.h"
#include "interrupts.h"
#include "int_handler_stubs.h"
#include "pic.h"

#include <kernel/segmentation/gdt_structures.h>
#include <kernel/assert.h>
#include <std/memory.h>

static void idt_set_gate64(idt_descriptor_t* entry, uintptr_t target_addr, bool ints_enabled) {
    // Trap gate if we want ints enabled, Interrupt gate otherwise
    uint8_t gate_type = (ints_enabled) ? 0xF : 0xE;
    idt_descriptor_t desc = {
        .entry_point_low = (target_addr & 0xFFFF),
        .kernel_code_segment_selector = 0x08,
        .ist = 0,
        .type = gate_type,
        .always_0 = 0,
        .ring_level = 0,
        .present = 1,
        .entry_point_mid = ((target_addr >> 16) & 0xFFFF),
        .entry_point_high = (target_addr >> 32)
    };
    memcpy(entry, &desc, sizeof(idt_descriptor_t));
}

static void idt_map_all_gates(idt_descriptor_t* idt_entries) {
    // The first 32 interrupt lines will be delivered from the CPU for exceptions
    idt_set_gate64(&idt_entries[0], (uintptr_t)internal_interrupt0, false);
    idt_set_gate64(&idt_entries[1], (uintptr_t)internal_interrupt1, false);
    idt_set_gate64(&idt_entries[2], (uintptr_t)internal_interrupt2, false);
    idt_set_gate64(&idt_entries[3], (uintptr_t)internal_interrupt3, false);
    idt_set_gate64(&idt_entries[4], (uintptr_t)internal_interrupt4, false);
    idt_set_gate64(&idt_entries[5], (uintptr_t)internal_interrupt5, false);
    idt_set_gate64(&idt_entries[6], (uintptr_t)internal_interrupt6, false);
    idt_set_gate64(&idt_entries[7], (uintptr_t)internal_interrupt7, false);
    idt_set_gate64(&idt_entries[8], (uintptr_t)internal_interrupt8, false);
    idt_set_gate64(&idt_entries[9], (uintptr_t)internal_interrupt9, false);
    idt_set_gate64(&idt_entries[10], (uintptr_t)internal_interrupt10, false);
    idt_set_gate64(&idt_entries[11], (uintptr_t)internal_interrupt11, false);
    idt_set_gate64(&idt_entries[12], (uintptr_t)internal_interrupt12, false);
    idt_set_gate64(&idt_entries[13], (uintptr_t)internal_interrupt13, false);
    idt_set_gate64(&idt_entries[14], (uintptr_t)internal_interrupt14, false);
    idt_set_gate64(&idt_entries[15], (uintptr_t)internal_interrupt15, false);
    idt_set_gate64(&idt_entries[16], (uintptr_t)internal_interrupt16, false);
    idt_set_gate64(&idt_entries[17], (uintptr_t)internal_interrupt17, false);
    idt_set_gate64(&idt_entries[18], (uintptr_t)internal_interrupt18, false);
    idt_set_gate64(&idt_entries[19], (uintptr_t)internal_interrupt19, false);
    idt_set_gate64(&idt_entries[20], (uintptr_t)internal_interrupt20, false);
    idt_set_gate64(&idt_entries[21], (uintptr_t)internal_interrupt21, false);
    idt_set_gate64(&idt_entries[22], (uintptr_t)internal_interrupt22, false);
    idt_set_gate64(&idt_entries[23], (uintptr_t)internal_interrupt23, false);
    idt_set_gate64(&idt_entries[24], (uintptr_t)internal_interrupt24, false);
    idt_set_gate64(&idt_entries[25], (uintptr_t)internal_interrupt25, false);
    idt_set_gate64(&idt_entries[26], (uintptr_t)internal_interrupt26, false);
    idt_set_gate64(&idt_entries[27], (uintptr_t)internal_interrupt27, false);
    idt_set_gate64(&idt_entries[28], (uintptr_t)internal_interrupt28, false);
    idt_set_gate64(&idt_entries[29], (uintptr_t)internal_interrupt29, false);
    idt_set_gate64(&idt_entries[30], (uintptr_t)internal_interrupt30, false);
    idt_set_gate64(&idt_entries[31], (uintptr_t)internal_interrupt31, false);
    idt_set_gate64(&idt_entries[ 0], (uintptr_t)internal_interrupt0, false);

    // Next 16 interrupt lines will be delivered from external devices via the PIC
    idt_set_gate64(&idt_entries[32], (uintptr_t)external_interrupt0, false);
    idt_set_gate64(&idt_entries[33], (uintptr_t)external_interrupt1, false);
    idt_set_gate64(&idt_entries[34], (uintptr_t)external_interrupt2, false);
    idt_set_gate64(&idt_entries[35], (uintptr_t)external_interrupt3, false);
    idt_set_gate64(&idt_entries[36], (uintptr_t)external_interrupt4, false);
    idt_set_gate64(&idt_entries[37], (uintptr_t)external_interrupt5, false);
    idt_set_gate64(&idt_entries[38], (uintptr_t)external_interrupt6, false);
    idt_set_gate64(&idt_entries[39], (uintptr_t)external_interrupt7, false);
    idt_set_gate64(&idt_entries[40], (uintptr_t)external_interrupt8, false);
    idt_set_gate64(&idt_entries[41], (uintptr_t)external_interrupt9, false);
    idt_set_gate64(&idt_entries[42], (uintptr_t)external_interrupt10, false);
    idt_set_gate64(&idt_entries[43], (uintptr_t)external_interrupt11, false);
    idt_set_gate64(&idt_entries[44], (uintptr_t)external_interrupt12, false);
    idt_set_gate64(&idt_entries[45], (uintptr_t)external_interrupt13, false);
    idt_set_gate64(&idt_entries[46], (uintptr_t)external_interrupt14, false);
    idt_set_gate64(&idt_entries[47], (uintptr_t)external_interrupt15, false);

    // Interrupt 128 used as a syscall vector
    idt_set_gate64(&idt_entries[128], (uintptr_t)internal_interrupt128, false);
}

void idt_init(void) {
    assert(sizeof(idt_descriptor_t) == 16, "Must be exactly 16 bytes!");

    static idt_descriptor_t idt_entries[256] = {0};
    static idt_pointer_t table = {0};

    table.table_base = (uintptr_t)&idt_entries;
    table.table_size = sizeof(idt_entries) - 1;

#define PIC_MASTER_OFFSET	0x20 //int 32 mapped to IRQ 0
#define PIC_SLAVE_OFFSET	0x28 //int 40+ mapped to IRQ8+
    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);

    idt_map_all_gates(idt_entries);
    idt_activate(&table);
}
