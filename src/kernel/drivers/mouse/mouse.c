#include "mouse.h"
#include <kernel/interrupts/interrupts.h>
#include <std/math.h>
#include <std/std.h>
#include <kernel/multitasking/tasks/task_small.h>
#include <kernel/syscall/sysfuncs.h>
#include <kernel/drivers/ps2/ps2.h>


#define PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS 0xF7
#define PS2_MOUSE_CMD_ENABLE_DATA_REPORTING 0xF4
#define PS2_MOUSE_RESP_ACKNOWLEDGE 0xFA

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static int mouse_callback(registers_t* regs) {
	uint8_t byte = ps2_read(PS2_DATA);
	amc_message_t* amc_msg = amc_message_construct__from_core(&byte, 1);
	amc_message_send("com.axle.mouse_driver", amc_msg);
	return 0;
}
#pragma GCC diagnostic pop

void ps2_mouse_enable(void) {
	printf_info("[PS2] Enabling mouse...");
	// Setup an interrupt handler to receive IRQ12's
	interrupt_setup_callback(INT_VECTOR_IRQ12, &mouse_callback);

	// Ask the PS/2 mouse to use default settings
	ps2_write_device(1, PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS);
    ps2_expect_ack();

	// Ask the PS/2 mouse to start sending events
	ps2_write_device(1, PS2_MOUSE_CMD_ENABLE_DATA_REPORTING);
    ps2_expect_ack();
}

void ps2_mouse_driver_launch(void) {
	// TODO(PT): Refactored method to launch a driver
    const char* program_name = "mouse_driver";
    FILE* fp = initrd_fopen(program_name, "rb");
    char* argv[] = {program_name, NULL};
    elf_load_file(program_name, fp, argv);
	panic("noreturn");
}

void mouse_event_wait() {
	Deprecated();
}

static inline Size screen_dimensions() {
	Deprecated();
}

Point mouse_point() {
	Deprecated();
}

uint8_t mouse_events() {
	Deprecated();
}

void mouse_reset_cursorpos() {
	Deprecated();
}

static void _mouse_constrain_to_screen_size() {
	Deprecated();
}

static void _mouse_handle_event(int x, int y) {
	Deprecated();
}