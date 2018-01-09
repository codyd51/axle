#include "descriptor_tables.h"
#include <std/std.h>
#include <kernel/assert.h>

//access ASM functions from C
extern void gdt_flush(uint32_t);
extern void idt_flush(uint32_t);
extern void tss_flush();

//internal function prototypes
static void init_gdt();
static void init_idt();
static void write_tss(int32_t, uint16_t, uint32_t);

tss_entry_t tss_entry;

static void gdt_install(void) {
	NotImplemented();
}

static void gdt_set_gate(int32_t a, uint32_t b, uint32_t c, uint8_t d, uint8_t e) {
	NotImplemented();
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void init_gdt() {
	//TODO: implement it
}
static void init_idt() {
	//TODO: implement it
}
#pragma GCC diagnostic pop

//initialize task state segment structure
static void write_tss(int32_t num, uint16_t ss0, uint32_t esp0) {
	//compute base and limit of GDT entry
	uint32_t base = (uint32_t)&tss_entry;
	uint32_t limit = base + sizeof(tss_entry);

	//add TSS descriptor's address to GDT
	gdt_set_gate(num, base, limit, 0xE9, 0x00);

	//ensure descriptor is empty
	memset(&tss_entry, 0, sizeof(tss_entry));

	//set kernel stack segment
	tss_entry.ss0 = ss0;
	//set kernel stack pointer
	tss_entry.esp0 = esp0;

	//set cs, ss, ds, es, fs, and gs entries in TSS
	//specify what segments should be loaded when processor switches to kernel mode
	//therefore, these are just normal kernel code/data segments: 0x08 and 0x10
	//but, with the last two bits set, making them 0x0b and 0x13
	//this sets the requested privilege level to 3, so this TSS can be used to switch
	//from kernel mode to ring3
	tss_entry.cs = 0x0b;
	tss_entry.ss = tss_entry.ds = tss_entry.es = tss_entry.fs = tss_entry.gs = 0x13;
}

void set_kernel_stack(uint32_t stack) {
	tss_entry.esp0 = stack;
}
