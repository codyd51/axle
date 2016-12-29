#include "kbman.h"
#include <kernel/drivers/terminal/terminal.h>
#include <std/array_m.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <std/kheap.h>
#include <kernel/drivers/kb/kb.h>

#define MAX_KB_BUFFER_LENGTH 64

static array_m* keys_down;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
void kbman_process(char c) {
	if (!keys_down) {
		keys_down = array_m_create(MAX_KB_BUFFER_LENGTH);
	}

	//only insert into array if it's not already present
	if (!key_down(c)) {
		array_m_insert(keys_down, (type_t)c);
	}
	
	key_status_t mods = kb_modifiers();

	//least significant bit of mods mask is control key
	//dispatch any ctrl+key keystrokes
	if (mods & 0x1) {
		switch (c) {
			//dump dynamic memory users whenever ctrl+m is pressed
			case 'm':
				memdebug();
				printf_info("Dynamic memory usage logged");
				break;
			default:
				break;
		}
	}

	//inform scheduler that a keystroke has been recieved
	//this call forces any tasks waiting on a keystroke to be woken
	//TODO this should take an arg for reason for waking tasks
	force_enumerate_blocked();
}

void kbman_process_release(char c) {
	//ensure all instances of this key are removed
	while (key_down(c)) {
		array_m_remove(keys_down, array_m_index(keys_down, (type_t)c));
	}
}

bool key_down(char c) {
	if (array_m_index(keys_down, (type_t)c) != ARR_NOT_FOUND) return true;
	return false;
}
#pragma GCC diagnostic push
