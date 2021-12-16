#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>
#include <kernel/adi.h>
#include <kernel/idt.h>

#include <libport/libport.h>
#include <libamc/libamc.h>
#include <libutils/array.h>

#include <awm/awm_messages.h>

#include "kb_driver.h"
#include "kb_colemak.h"
#include "kb_driver_messages.h"

static void emit_key_event(ps2_kbd_state_t* state, uint32_t key, key_event_type_t type) {
	key_event_t* e = calloc(1, sizeof(key_event_t));
	e->type = type;
	e->key = key;
	//printf("Emit 0x%02x (%c) %s\n", key, key, type == KEY_PRESSED ? "pressed" : "released");
	array_insert(state->key_event_stream, e);
}

static int32_t _get_key_ident_for_scancode(ps2_kbd_state_t* state, uint8_t scancode) {
	if (scancode == ARROW_UP) {
		return KEY_IDENT_UP_ARROW;
	}
	else if (scancode == ARROW_DOWN) {
		return KEY_IDENT_DOWN_ARROW;
	}
	else if (scancode == ARROW_LEFT) {
		return KEY_IDENT_LEFT_ARROW;
	}
	else if (scancode == ARROW_RIGHT) {
		return KEY_IDENT_RIGHT_ARROW;
	}
	else if (scancode == SHIFT_LEFT) {
		return KEY_IDENT_LEFT_SHIFT;
	}
	else if (scancode == SHIFT_RIGHT) {
		return KEY_IDENT_RIGHT_SHIFT;
	}
	else if (scancode == ESCAPE) {
		return KEY_IDENT_ESCAPE;
	}
	else if (scancode == LEFT_CONTROL) {
		return KEY_IDENT_LEFT_CONTROL;
	}
	else if (scancode == LEFT_COMMAND) {
		return KEY_IDENT_LEFT_COMMAND;
	}
	else if (scancode == LEFT_OPTION) {
		return KEY_IDENT_LEFT_OPTION;
	}

	uint32_t scancode_map_size = sizeof(state->layout->scancode_idx_to_ord) / sizeof(state->layout->scancode_idx_to_ord[0]);
	if (scancode < scancode_map_size) {
		uint8_t char_value = state->layout->scancode_idx_to_ord[scancode];
		if (char_value > 0) {
			return char_value;
		}
	}
	return -1;
}

static void _process_scancode(ps2_kbd_state_t* state, uint8_t scancode) {
	// https://www.nutsvolts.com/magazine/article/get-ascii-data-from-ps-2-keyboards
	//printf("scancode 0%02x\n", scancode);
	if (scancode == SCANCODE_SEQUENCE_BEGIN) {
		state->is_processing_scancode_sequence = true;
		return;
	}

	if (scancode == SCANCODE_KEY_RELEASED) {
		state->is_processing_key_release = true;
		return;
	}

	int32_t key_ident = _get_key_ident_for_scancode(state, scancode);
	if (key_ident > 0) {
		key_event_type_t type = state->is_processing_key_release ? KEY_RELEASED : KEY_PRESSED;
		emit_key_event(state, key_ident, type);
		if (state->is_processing_key_release) {
			state->is_processing_key_release = false;
			state->is_processing_scancode_sequence = false;
		}
	}
	else {
		printf("Failed to map scancode %d\n", scancode);
	}
}

static void _handle_amc_messages(void) {
	if (!amc_has_message()) {
		return;
	}
	do {
		amc_message_t* msg;
		amc_message_await_any(&msg);
		if (!libamc_handle_message(msg)) {
			printf("com.axle.kb_driver received unknown amc message from %s\n", msg->source);
		}
	} while (amc_has_message());
}

int main(int argc, char** argv) {
	amc_register_service(KB_DRIVER_SERVICE_NAME);
	// This process will handle PS/2 keyboard IRQ's (IRQ 1)
	adi_register_driver(KB_DRIVER_SERVICE_NAME, INT_VECTOR_IRQ1);

	ps2_kbd_state_t state = {0};
	// TODO(PT): A knob that allows you to set the active layout to QWERTY
	state.layout = &colemak;
	state.key_event_stream = array_create(64);

	while (true) {
		// Await an interrupt from the PS/2 keyboard
		bool awoke_for_interrupt = adi_event_await(INT_VECTOR_IRQ1);
		if (!awoke_for_interrupt) {
			_handle_amc_messages();
			continue;
		}

		// An interrupt is ready to be serviced!
		// TODO(PT): Copy the PS2 header to the sysroot as a build step, 
		// and replace this port number with PS2_DATA
		uint8_t scancode = inb(0x60);
		adi_send_eoi(INT_VECTOR_IRQ1);

		_process_scancode(&state, scancode);
		// Have we generated a new key event?
		if (state.key_event_stream->size) {
			for (uint32_t i = 0; i < state.key_event_stream->size; i++) {
				key_event_t* e = array_lookup(state.key_event_stream, i);
				amc_message_send(AWM_SERVICE_NAME, e, sizeof(key_event_t));
			}
			for (uint32_t i = 0; i < state.key_event_stream->size; i++) {
				key_event_t* e = array_lookup(state.key_event_stream, i);
				array_remove(state.key_event_stream, i);
				free(e);
			}
		}
	}
	return 0;
}
