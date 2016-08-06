#include "kbman.h"
#include <kernel/drivers/terminal/terminal.h>
#include <std/array_m.h>
#include <stdbool.h>

#define KEY_UP 		0x48
#define KEY_DOWN 	0x50
#define KEY_LEFT 	0x4B
#define KEY_RIGHT 	0x4D

static array_m* keys_down;
void kbman_process(char c) {
	if (!keys_down) {
		keys_down = array_m_create(32);
	}

	array_m_insert(keys_down, c);

	if (c == KEY_UP) {
		term_scroll(TERM_SCROLL_UP);	
	}
	else if (c == KEY_DOWN) {
		term_scroll(TERM_SCROLL_DOWN);
	}
}

void kbman_process_release(char c) {
	array_m_remove(keys_down, c);	
}

bool key_down(char c) {
	if (array_m_index(keys_down, c) != -1) return true;
	return false;
}
