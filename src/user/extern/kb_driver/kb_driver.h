#ifndef KB_DRIVER_H
#define KB_DRIVER_H

#include <stdint.h>

#define SCANCODE_KEY_RELEASED		0xF0
#define SCANCODE_SHIFT_L_PRESSED	0x12
#define SCANCODE_SHIFT_R_PRESSED	0x59

typedef struct scancode_layout {
	// Indices correspond to the scancode recevied from the PS/2 keyboard device
	// Values correspond to the ASCII character to be input
	// Generate the array literal using generate_keyboard_map.py
	uint8_t scancode_idx_to_ord[256];
} scancode_layout_t;

typedef struct ps2_kbd_state {
	bool is_processing_key_release;
	bool is_shift_held;
	scancode_layout_t* layout;
} ps2_kbd_state_t;

#endif
