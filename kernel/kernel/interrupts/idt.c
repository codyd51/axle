#include "idt.h"
#include "idt_structures.h"
#include "interrupts.h"
#include "int_handler_stubs.h"
#include "pic.h"

#include <kernel/segmentation/gdt_structures.h>
#include <kernel/assert.h>
#include <std/memory.h>

static idt_descriptor_t _g_idt_entries[256] = {0};
static idt_pointer_t _g_idt_pointer = {0};

static void idt_set_gate64_ext(idt_descriptor_t* entry, uintptr_t target_addr, bool ints_enabled, bool accessible_from_usermode) {
    // Trap gate if we want ints enabled, Interrupt gate otherwise
    uint8_t gate_type = (ints_enabled) ? 0xF : 0xE;
    idt_descriptor_t desc = {
        .entry_point_low = (target_addr & 0xFFFF),
        .kernel_code_segment_selector = 0x08,
        .ist = 0,
        .type = gate_type,
        .always_0 = 0,
        .ring_level = (accessible_from_usermode) ?  3 : 0,
        .present = 1,
        .entry_point_mid = ((target_addr >> 16) & 0xFFFF),
        .entry_point_high = (target_addr >> 32)
    };
    memcpy(entry, &desc, sizeof(idt_descriptor_t));
}

static void idt_set_gate64(idt_descriptor_t* entry, uintptr_t target_addr) {
    idt_set_gate64_ext(entry, target_addr, false, false);
}

static void idt_map_all_gates(idt_descriptor_t* idt_entries) {
    // The first 32 interrupt lines will be delivered from the CPU for exceptions
    idt_set_gate64(&idt_entries[0], (uintptr_t)internal_interrupt0);
    idt_set_gate64(&idt_entries[1], (uintptr_t)internal_interrupt1);
    idt_set_gate64(&idt_entries[2], (uintptr_t)internal_interrupt2);
    idt_set_gate64(&idt_entries[3], (uintptr_t)internal_interrupt3);
    idt_set_gate64(&idt_entries[4], (uintptr_t)internal_interrupt4);
    idt_set_gate64(&idt_entries[5], (uintptr_t)internal_interrupt5);
    idt_set_gate64(&idt_entries[6], (uintptr_t)internal_interrupt6);
    idt_set_gate64(&idt_entries[7], (uintptr_t)internal_interrupt7);
    idt_set_gate64(&idt_entries[8], (uintptr_t)internal_interrupt8);
    idt_set_gate64(&idt_entries[9], (uintptr_t)internal_interrupt9);
    idt_set_gate64(&idt_entries[10], (uintptr_t)internal_interrupt10);
    idt_set_gate64(&idt_entries[11], (uintptr_t)internal_interrupt11);
    idt_set_gate64(&idt_entries[12], (uintptr_t)internal_interrupt12);
    idt_set_gate64(&idt_entries[13], (uintptr_t)internal_interrupt13);
    idt_set_gate64(&idt_entries[14], (uintptr_t)internal_interrupt14);
    idt_set_gate64(&idt_entries[15], (uintptr_t)internal_interrupt15);
    idt_set_gate64(&idt_entries[16], (uintptr_t)internal_interrupt16);
    idt_set_gate64(&idt_entries[17], (uintptr_t)internal_interrupt17);
    idt_set_gate64(&idt_entries[18], (uintptr_t)internal_interrupt18);
    idt_set_gate64(&idt_entries[19], (uintptr_t)internal_interrupt19);
    idt_set_gate64(&idt_entries[20], (uintptr_t)internal_interrupt20);
    idt_set_gate64(&idt_entries[21], (uintptr_t)internal_interrupt21);
    idt_set_gate64(&idt_entries[22], (uintptr_t)internal_interrupt22);
    idt_set_gate64(&idt_entries[23], (uintptr_t)internal_interrupt23);
    idt_set_gate64(&idt_entries[24], (uintptr_t)internal_interrupt24);
    idt_set_gate64(&idt_entries[25], (uintptr_t)internal_interrupt25);
    idt_set_gate64(&idt_entries[26], (uintptr_t)internal_interrupt26);
    idt_set_gate64(&idt_entries[27], (uintptr_t)internal_interrupt27);
    idt_set_gate64(&idt_entries[28], (uintptr_t)internal_interrupt28);
    idt_set_gate64(&idt_entries[29], (uintptr_t)internal_interrupt29);
    idt_set_gate64(&idt_entries[30], (uintptr_t)internal_interrupt30);
    idt_set_gate64(&idt_entries[31], (uintptr_t)internal_interrupt31);

    // Next 16 interrupt lines will be delivered from external devices via the APIC
    idt_set_gate64(&idt_entries[32], (uintptr_t)external_interrupt0);
    idt_set_gate64(&idt_entries[33], (uintptr_t)external_interrupt1);
    idt_set_gate64(&idt_entries[34], (uintptr_t)external_interrupt2);
    idt_set_gate64(&idt_entries[35], (uintptr_t)external_interrupt3);
    idt_set_gate64(&idt_entries[36], (uintptr_t)external_interrupt4);
    idt_set_gate64(&idt_entries[37], (uintptr_t)external_interrupt5);
    idt_set_gate64(&idt_entries[38], (uintptr_t)external_interrupt6);
    idt_set_gate64(&idt_entries[39], (uintptr_t)external_interrupt7);
    idt_set_gate64(&idt_entries[40], (uintptr_t)external_interrupt8);
    idt_set_gate64(&idt_entries[41], (uintptr_t)external_interrupt9);
    idt_set_gate64(&idt_entries[42], (uintptr_t)external_interrupt10);
    idt_set_gate64(&idt_entries[43], (uintptr_t)external_interrupt11);
    idt_set_gate64(&idt_entries[44], (uintptr_t)external_interrupt12);
    idt_set_gate64(&idt_entries[45], (uintptr_t)external_interrupt13);
    idt_set_gate64(&idt_entries[46], (uintptr_t)external_interrupt14);
    idt_set_gate64(&idt_entries[47], (uintptr_t)external_interrupt15);

    idt_set_gate64(&idt_entries[64], (uintptr_t)external_interrupt16);
    idt_set_gate64(&idt_entries[65], (uintptr_t)external_interrupt17);
    idt_set_gate64(&idt_entries[66], (uintptr_t)external_interrupt18);
    idt_set_gate64(&idt_entries[67], (uintptr_t)external_interrupt19);
    idt_set_gate64(&idt_entries[68], (uintptr_t)external_interrupt20);
    idt_set_gate64(&idt_entries[69], (uintptr_t)external_interrupt21);
    idt_set_gate64(&idt_entries[70], (uintptr_t)external_interrupt22);
    idt_set_gate64(&idt_entries[71], (uintptr_t)external_interrupt23);
    idt_set_gate64(&idt_entries[72], (uintptr_t)external_interrupt24);
    idt_set_gate64(&idt_entries[73], (uintptr_t)external_interrupt25);
    idt_set_gate64(&idt_entries[74], (uintptr_t)external_interrupt26);
    idt_set_gate64(&idt_entries[75], (uintptr_t)external_interrupt27);
    idt_set_gate64(&idt_entries[76], (uintptr_t)external_interrupt28);
    idt_set_gate64(&idt_entries[77], (uintptr_t)external_interrupt29);
    idt_set_gate64(&idt_entries[78], (uintptr_t)external_interrupt30);
    idt_set_gate64(&idt_entries[79], (uintptr_t)external_interrupt31);
    idt_set_gate64(&idt_entries[80], (uintptr_t)external_interrupt32);
    idt_set_gate64(&idt_entries[81], (uintptr_t)external_interrupt33);
    idt_set_gate64(&idt_entries[82], (uintptr_t)external_interrupt34);
    idt_set_gate64(&idt_entries[83], (uintptr_t)external_interrupt35);
    idt_set_gate64(&idt_entries[84], (uintptr_t)external_interrupt36);
    idt_set_gate64(&idt_entries[85], (uintptr_t)external_interrupt37);
    idt_set_gate64(&idt_entries[86], (uintptr_t)external_interrupt38);
    idt_set_gate64(&idt_entries[87], (uintptr_t)external_interrupt39);
    idt_set_gate64(&idt_entries[88], (uintptr_t)external_interrupt40);
    idt_set_gate64(&idt_entries[89], (uintptr_t)external_interrupt41);
    idt_set_gate64(&idt_entries[90], (uintptr_t)external_interrupt42);
    idt_set_gate64(&idt_entries[91], (uintptr_t)external_interrupt43);
    idt_set_gate64(&idt_entries[92], (uintptr_t)external_interrupt44);
    idt_set_gate64(&idt_entries[93], (uintptr_t)external_interrupt45);
    idt_set_gate64(&idt_entries[94], (uintptr_t)external_interrupt46);
    idt_set_gate64(&idt_entries[95], (uintptr_t)external_interrupt47);
    idt_set_gate64(&idt_entries[96], (uintptr_t)external_interrupt48);

    // Interrupt 128 used as a syscall vector
    idt_set_gate64_ext(&idt_entries[128], (uintptr_t)internal_interrupt128, false, true);
}

void idt_init(void) {
    assert(sizeof(idt_descriptor_t) == 16, "Must be exactly 16 bytes!");

    _g_idt_pointer.table_base = (uintptr_t)&_g_idt_entries;
    _g_idt_pointer.table_size = sizeof(_g_idt_entries) - 1;

#define PIC_MASTER_OFFSET	0x20 //int 32 mapped to master PIC interrupt line 0
#define PIC_SLAVE_OFFSET	0x28 //int 40+ mapped to slave PIC interrupt line 0 (which is sent as PIC IRQ 8+)
    pic_remap(PIC_MASTER_OFFSET, PIC_SLAVE_OFFSET);

    idt_map_all_gates(_g_idt_entries);
    idt_activate(&_g_idt_pointer);
}

idt_pointer_t* kernel_idt_pointer(void) {
    return &_g_idt_pointer;
}

void registers_print(register_state_t* r) {
	printf("- [%d] Registers at 0x%p -\n", getpid(), r->return_rip);
	printf("\trax 0x%p rbx 0x%p rcx 0x%p rdx 0x%p\n", r->rax, r->rbx, r->rcx, r->rdx);
	printf("\trsi 0x%p rdi 0x%p rbp 0x%p rsp 0x%p\n", r->rsi, r->rdi, r->rbp, r->return_rsp);
	printf("\tr8  0x%p r9  0x%p r10 0x%p r11 0x%p\n", r->r8, r->r9, r->r10, r->r11);
	printf("\tr12 0x%p r13 0x%p r14 0x%p r15 0x%p\n", r->r12, r->r13, r->r14, r->r15);
	printf("\trip 0x%p rfl 0x%08x\n", r->return_rip, r->rflags);
}
