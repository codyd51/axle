#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <kernel/amc.h>

#include "kb_driver.h"
#include "kb_us.h"
#include "kb_colemak.h"

static keymap_t* _active_kb_layout = 0;

static void _set_active_kb_layout(keymap_t* new_layout) {
	_active_kb_layout = new_layout;
}

key_status_t kb_modifiers() {
	return _active_kb_layout->controls;
}

static char _translate_scancode_to_character(uint8_t scancode) {
	// Has a key just been released?
	if (scancode & RELEASED_MASK) {
		// First 5 bits store modifier flags
		for (int i = 0; i < 5; i++) {
			if (_active_kb_layout->control_map[i] == (scancode & ~RELEASED_MASK)) {
				// Clear the status flag associated with the modifier key that was released
				_active_kb_layout->controls &= ~(1 << i);
				// We could inform axle here that a modifier key was released
				return 0;
			}
		}

		// Clear released bit from the scancode to read the key that was released
		scancode &= ~RELEASED_MASK;
		// We could inform axle here that a regular key was released
		return 0;
	}

	// If a modifier key has just been pressed, 
	// set a status flag for it in the keyboard state
	for (int i = 0; i < 8; i++) {
		if (_active_kb_layout->control_map[i] == scancode) {
			// Unset the bit if it was set already
			// PT: How would this happen?
			if (_active_kb_layout->controls & 1 << i) {
				_active_kb_layout->controls &= ~(1 << i);
			}
			// Enable the bit since the modifier key is now pressed
			else {
				_active_kb_layout->controls |= 1 << i;
			}

			// We could inform axle here that a modifier key has just been pressed
			return 0;
		}
	}

	// Regular key press
	// Read uppercase/lowercase version based on modifier key status
	uint8_t* scancodes = _active_kb_layout->scancodes;
	if ((_active_kb_layout->controls & (LSHIFT | RSHIFT | CAPSLOCK)) && !(_active_kb_layout->controls & CONTROL)) {
		scancodes = _active_kb_layout->shift_scancodes;
	}

	// Translate the scancode based on the active keyboard layout
	char pressed_key = scancodes[scancode];
	return pressed_key;
}

int main(int argc, char** argv) {
	amc_register_service("com.axle.kb_driver");
	_set_active_kb_layout(&kb_colemak);

	while (true) {
		// The message from the low-level keyboard driver will contain the bare scancode
		amc_message_t msg = {0};
		amc_message_await("com.axle.core", &msg);
		uint8_t scancode = msg.data[0];
		char translated_key = _translate_scancode_to_character(scancode);
		// Ignore the keypress if it wasn't a newly pressed regular character
		if (translated_key) {
			amc_message_t* keypress_msg = amc_message_construct(&translated_key, 1);
			//amc_message_broadcast(keypress_msg);
			amc_message_send("com.axle.awm", keypress_msg);
		}
	}
	
	return 0;
}
