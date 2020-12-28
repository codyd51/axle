#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>

#include "kb_driver.h"
#include "kb_colemak.h"

static int process_scancode(ps2_kbd_state_t* state, uint8_t scancode) {
	// https://www.nutsvolts.com/magazine/article/get-ascii-data-from-ps-2-keyboards
	if (scancode == SCANCODE_KEY_RELEASED) {
		//assert(!state->is_processing_key_release, "Received a key release while processing a key release");
		state->is_processing_key_release = true;
		return 0;
	}
	else if (scancode == SCANCODE_SHIFT_L_PRESSED || scancode == SCANCODE_SHIFT_R_PRESSED) {
		if (state->is_processing_key_release) {
			// Shift key was just released
			state->is_processing_key_release = false;
			state->is_shift_held = false;
			return 0;
		}
		else {
			// Shift key was just pressed
			state->is_shift_held = true;
			return 0;
		}
	}
	else {
		// Add the new scan code to the built-up buffer
		if (state->is_processing_key_release) {
			// Just released a key, and we know what key is was
			state->is_processing_key_release = false;
			return 0;
		}
		else {
			uint32_t scancode_map_size = sizeof(state->layout->scancode_idx_to_ord) / sizeof(state->layout->scancode_idx_to_ord[0]);
			if (scancode < scancode_map_size) {
				uint8_t char_value = state->layout->scancode_idx_to_ord[scancode];
				if (char_value > 0) {
					const char* desc = state->is_shift_held ? "upper" : "lower";
					printf("Keypress %s %c\n", desc, char_value);
					return char_value;
				}
				else {
					printf("Unmapped scancode 0x%02x\n", scancode);
					return 0;
				}
			}
			else {
				printf("Scancode larger than map 0x%02x\n", scancode);
				return 0;
			}
		}
	}
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.kb_driver");
	ps2_kbd_state_t state = {0};
	// TODO(PT): A knob that allows you to set the active layout to QWERTY
	state.layout = &colemak;

	while (true) {
		// The message from the low-level keyboard driver will contain the bare scancode
		amc_message_t msg = {0};
		amc_message_await("com.axle.core", &msg);
		uint8_t scancode = msg.data[0];
		uint8_t char_value = process_scancode(&state, scancode);
		// Were we able to map a scancode?
		if (char_value > 0) {
			amc_message_t* keypress_msg = amc_message_construct(&char_value, 1);
			amc_message_send("com.axle.awm", keypress_msg);
		}
		/*
		if (translated_key) {
			amc_message_t* keypress_msg = amc_message_construct(&translated_key, 1);
			//amc_message_broadcast(keypress_msg);
			amc_message_send("com.axle.awm", keypress_msg);
		}
		*/
	}
	
	return 0;
}
