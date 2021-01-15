#include "mouse.h"
#include <std/math.h>
#include <std/std.h>
#include <kernel/util/vfs/fs.h>
#include <kernel/util/adi/adi.h>
#include <kernel/drivers/ps2/ps2.h>
#include <kernel/interrupts/idt.h>
#include <kernel/interrupts/interrupts.h>

#define PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS 0xF6
#define PS2_MOUSE_CMD_ENABLE_DATA_REPORTING 0xF4
#define PS2_MOUSE_RESP_ACKNOWLEDGE 0xFA

void ps2_mouse_enable(void) {
	printf_info("[PS2] Enabling mouse...");
	uint8_t mouse_id = ps2_read(PS2_DATA);
	printf_info("[PS2] Mouse ID: 0x%02x", mouse_id);

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
	return size_make(0, 0);
}

Point mouse_point() {
	Deprecated();
	return point_make(0, 0);
}

uint8_t mouse_events() {
	Deprecated();
	return 0;
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
