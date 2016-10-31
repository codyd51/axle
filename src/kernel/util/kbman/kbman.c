#include "kbman.h"
#include <kernel/drivers/terminal/terminal.h>
#include <std/array_m.h>
#include <kernel/util/multitasking/tasks/task.h>

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

	//dispatch key if necessary
	switch (c) {
		default:
			break;
	}

	//inform scheduler that a keystroke has been recieved
	//this call forces any tasks waiting on a keystroke to be woken
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
