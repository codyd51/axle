#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>
#include <kernel/adi.h>
#include <kernel/idt.h>

#include <libport/libport.h>

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
	// This process will handle PS/2 keyboard IRQ's (IRQ 1)
	adi_register_driver("com.axle.kb_driver", INT_VECTOR_IRQ1);
	amc_register_service("com.axle.kb_driver");

	ps2_kbd_state_t state = {0};
	// TODO(PT): A knob that allows you to set the active layout to QWERTY
	state.layout = &colemak;

	while (true) {
		// Await an interrupt from the PS/2 keyboard
		bool awoke_for_interrupt = adi_event_await(INT_VECTOR_IRQ1);
		if (!awoke_for_interrupt) {
			printf("com.axle.kb_driver woke for an amc message... why?\n");
			// Eat the message
			amc_message_t* t;
			amc_message_await_any(&t);
			continue;
		}

		// An interrupt is ready to be serviced!
		// TODO(PT): Copy the PS2 header to the sysroot as a build step, 
		// and replace this port number with PS2_DATA
		uint8_t scancode = inb(0x60);
		adi_send_eoi(INT_VECTOR_IRQ1);

		uint8_t char_value = process_scancode(&state, scancode);
		// Were we able to map a scancode?
		if (char_value > 0) {
			// Send a message to the window server telling it about the keystroke
			uint8_t buf[1] = {char_value};
			amc_message_construct_and_send("com.axle.awm", &buf, 1);
		}
	}
	return 0;
}
