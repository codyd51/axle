#include "timer.h"
#include <limits.h>
#include <std/memory.h>
#include <std/common.h>
#include <kernel/drivers/pit/pit.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/syscall/sysfuncs.h>
#include <kernel/interrupts/int_notifier.h>

static int callback_num = 0;
static timer_callback_t callback_table[MAX_CALLBACKS] = {0};

static void clear_table() {
	memset(&callback_table, 0, sizeof(timer_callback_t) * callback_num);
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

timer_callback_t* timer_callback_register(void* func, int interval, bool repeats, void* context) {
	int next_open_index = next_open_callback_index();
	//only add callback if we have room
	if (callback_num + 1 < MAX_CALLBACKS || next_open_index < callback_num) {
		callback_table[callback_num].func = func;
		callback_table[callback_num].interval = interval;
		callback_table[callback_num].time_left = interval;
		callback_table[callback_num].repeats = repeats;
		callback_table[callback_num].context = context;

		callback_num++;

		return &(callback_table[callback_num]);
	}

	panic("timer callback table out of space");
	return NULL;
}

void timer_callback_remove(timer_callback_t* callback) {
	//find this callback in callback table
	bool found = false;
	for (int i = 0; i < callback_num; i++) {
		if (callback_table[callback_num].func == callback->func) {
			found = true;
			break;
		}
	}
	if (!found) {
		panic("tried to delete nonexistant timer callback. investigate!");
	}
	memset(callback, 0, sizeof(timer_callback_t));
}

void _timer_handle_pit_tick() {
	//look through every callback and see if we should fire
	for (int i = 0; i < callback_num; i++) {
		//decrement time left
		callback_table[i].time_left -= 1;

		//if it's time to fire, do so
		if (callback_table[i].time_left <= 0) {
			//reset for next firing
			callback_table[i].time_left = callback_table[i].interval;

			void(*callback_func)(void*) = (void(*)(void*))callback_table[i].func;
			callback_func(callback_table[i].context);

			//if we only fire once, trash this callback
			if (!callback_table[i].repeats) {
				timer_callback_remove(&(callback_table[i]));
			}
		}
	}
}

void timer_callback_deliver_immediately(timer_callback_t* callback) {
	callback->time_left = 0;
}

void timer_init() {
	int_notifier_register_callback(PIT_INT_VECTOR, _timer_handle_pit_tick, NULL, true);
}
