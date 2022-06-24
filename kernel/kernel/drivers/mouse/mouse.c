#include "mouse.h"
#include <std/std.h>
#include <kernel/drivers/ps2/ps2.h>

#define PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS 0xF6
#define PS2_MOUSE_CMD_ENABLE_DATA_REPORTING 0xF4
#define PS2_MOUSE_RESP_ACKNOWLEDGE 0xFA

static void _ps2_set_sample_rate(uint8_t samples_per_second) {
	ps2_write_device(1, 0xF3);
    ps2_expect_ack();
	ps2_write_device(1, samples_per_second);
	ps2_expect_ack();
}

void ps2_mouse_enable(void) {
	printf("[PS2] Enabling mouse...\n");
	uint8_t mouse_id = ps2_read(PS2_DATA);
	printf("[PS2] Initial mouse ID: 0x%02x\n", mouse_id);

	// Ask the PS/2 mouse to use default settings
	ps2_write_device(1, PS2_MOUSE_CMD_SET_DEFAULT_SETTINGS);
    ps2_expect_ack();

	printf("[PS2] Enabling scroll wheel...\n");
	_ps2_set_sample_rate(200);
	_ps2_set_sample_rate(100);
	_ps2_set_sample_rate(80);
	ps2_write_device(1, 0xF2);
	ps2_expect_ack();
	mouse_id = ps2_read(PS2_DATA);
	printf("[PS2] New mouse ID: 0x%02x\n", mouse_id);
	assert(mouse_id == 0x03, "Failed to enable scroll wheel");
	_ps2_set_sample_rate(60);

	// Ask the PS/2 mouse to start sending events
	ps2_write_device(1, PS2_MOUSE_CMD_ENABLE_DATA_REPORTING);
    ps2_expect_ack();
}

void mouse_event_wait() {
	Deprecated();
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
