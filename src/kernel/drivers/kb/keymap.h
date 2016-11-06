#ifndef KEYMAP_H
#define KEYMAP_H

//1 bit per control key, in order above (LSB = CONTROL, MSB = NUMLOCK)
//1 == set, 0 == not set
typedef uint8_t key_status_t;

typedef struct keymap {
	//chars mapped to scancodes
	uint8_t scancodes[128];
	uint8_t shift_scancodes[128];

	//function keys mapped to bit positions in key status map
	uint8_t control_map[8];

	//statuses of control keys
	key_status_t controls;
} keymap_t;

#endif
