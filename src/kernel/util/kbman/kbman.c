#include "kbman.h"
#include <kernel/drivers/terminal/terminal.h>
#include <std/array_m.h>

static array_m* keys_down;
void kbman_process(char c) {
	if (!keys_down) {
		keys_down = array_m_create(32);
	}

	//only insert into array if it's not already present
	if (!key_down(c)) {
		array_m_insert(keys_down, (type_t*)c);
	}

	//dispatch key if necessary
	switch (c) {
		default:
			break;
	}
}

void kbman_process_release(char c) {
	//ensure all instances of this key are removed
	while (key_down(c)) {
		array_m_remove(keys_down, array_m_index(keys_down, (type_t*)c));	
	}
}

bool key_down(char c) {
	if (array_m_index(keys_down, (type_t*)c) != -1) return true;
	return false;
}
