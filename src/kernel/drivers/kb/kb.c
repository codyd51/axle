#include "kb.h"
#include <std/common.h>
#include <kernel/interrupts/interrupts.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/vfs/fs.h>

char kgetch() {
	Deprecated();
	return '\0';
}

char getchar() {
	Deprecated();
	return '\0';
}

bool haskey() {
	Deprecated();
	return false;
}

char kb_modifiers() {
	Deprecated();
	return 0;
}

void kb_callback(registers_t* regs) {
	uint8_t scancode = inb(0x60);
	amc_message_t* amc_msg = amc_message_construct__from_core(STDOUT, &scancode, 1);
	amc_message_send("com.axle.kb_driver", amc_msg);
}

void kb_init() {
	printf_info("Initializing keyboard driver...");
	interrupt_setup_callback(INT_VECTOR_IRQ1, &kb_callback);

	// TODO(PT): Refactored method to launch a driver
    const char* program_name = "kb_driver";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
}
