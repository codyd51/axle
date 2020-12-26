#include "kb.h"
#include <std/common.h>
#include <kernel/interrupts/interrupts.h>
#include <kernel/util/amc/amc.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/drivers/ps2/ps2.h>

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

// TODO(PT): If a higher priority task comes in, context switch

void kb_callback(registers_t* regs) {
	uint8_t scancode = ps2_read(PS2_DATA);
	printf("PS2 keyboard scancode 0x%08x\n", scancode);
	amc_message_t* amc_msg = amc_message_construct__from_core(&scancode, 1);
	amc_message_send("com.axle.kb_driver", amc_msg);
}

void ps2_keyboard_enable(void) {
	printf_info("[PS2] Enabling keyboard...");
	// Setup an interrupt handler to receive IRQ1's
	interrupt_setup_callback(INT_VECTOR_IRQ1, &kb_callback);

	// Ask the PS/2 keyboard to start sending events
	ps2_write_device(0, PS2_DEV_ENABLE_SCAN);
	// TODO(PT): Is this ack actually sent as an interrupt?
	ps2_expect_ack();
}

void ps2_keyboard_driver_launch(void) {
	// TODO(PT): Refactored method to launch a driver
    const char* program_name = "kb_driver";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
}