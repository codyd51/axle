#ifndef KB_DRIVER_H
#define KB_DRIVER_H

#include <stdint.h>

#define CONTROL		0x1
#define ALT			0x2
#define ALTGR		0x4
#define LSHIFT		0x8
#define RSHIFT		0x10
#define CAPSLOCK	0x20
#define SCROLLLOCK	0x40
#define NUMLOCK		0x80

#define RELEASED_MASK 0x80

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
