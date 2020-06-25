#include "int_notifier.h"

#include <limits.h>
#include <std/memory.h>
#include <std/common.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/syscall/sysfuncs.h>

static int callback_num = 0;
static int_notify_callback_t callback_table[MAX_CALLBACKS] = {0};

static void clear_table() {
	memset(&callback_table, 0, sizeof(int_notify_callback_t) * callback_num);
	callback_num = 0;
}

static int next_open_callback_index() {
	for (int i = 0; i < callback_num; i++) {
		if (!callback_table[i].func) {
			//this index doesn't have valid data, fit for reuse
			return i;
		}
	}
	//all indexes up to callback_num are in use, so return callback_num
	return callback_num;
}

int_notify_callback_t* int_notifier_register_callback(uint32_t int_no, void* func, void* context, bool repeats) {
	int next_open_index = next_open_callback_index();
	//only add callback if we have room
	if (callback_num + 1 < MAX_CALLBACKS || next_open_index < callback_num) {
        callback_table[callback_num].int_no = int_no;
		callback_table[callback_num].func = func;
		callback_table[callback_num].context = context;
		callback_table[callback_num].repeats = repeats;

		callback_num++;

		return &(callback_table[callback_num]);
	}

    panic("int callback table out of space");
	return NULL;
}

void int_notifier_remove_callback(int_notify_callback_t* callback) {
	//find this callback in callback table
	bool found = false;
	for (int i = 0; i < callback_num; i++) {
		if (callback_table[callback_num].func == callback->func && callback_table[callback_num].int_no == callback->int_no) {
			found = true;
			break;
		}
	}
	if (!found) {
		panic("tried to delete nonexistant interrupt callback. investigate!");
	}
	memset(callback, 0, sizeof(int_notify_callback_t));
}

void int_notifier_handle_interrupt(registers_t* register_state) {
	//look through every callback and see if we should fire
	for (int i = 0; i < callback_num; i++) {
        // if this callback is waiting for the interrupt that was fired, invoke the callback
        if (callback_table[i].int_no == register_state->int_no) {
			void(*callback_func)(void*) = (void(*)(void*))callback_table[i].func;
			callback_func(callback_table[i].context);

			//if we only fire once, trash this callback
			if (!callback_table[i].repeats) {
				int_notifier_remove_callback(&(callback_table[i]));
			}
		}
	}
}