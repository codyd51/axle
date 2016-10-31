#include "timer.h"
#include <limits.h>
#include <std/memory.h>
#include <kernel/drivers/rtc/clock.h>
#include <kernel/util/syscall/sysfuncs.h>

int callback_num;
static timer_callback callback_table[MAX_CALLBACKS];

static void clear_table() {
	memset(&callback_table, 0, sizeof(timer_callback) * callback_num);
	callback_num = 0;
}

static int next_open_callback_index() {
	for (int i = 0; i < callback_num; i++) {
		if (!callback_table[i].callback) {
			//this index doesn't have valid data, fit for reuse
			return i;
		}
	}
	//all indexes up to callback_num are in use, so return callback_num
	return callback_num;
}

timer_callback add_callback(void* callback, int interval, bool repeats, void* context) {
	int next_open_index = next_open_callback_index();
	//only add callback if we have room
	if (callback_num + 1 < MAX_CALLBACKS || next_open_index < callback_num) {
		callback_table[callback_num].callback = callback;
		callback_table[callback_num].interval = interval;
		callback_table[callback_num].time_left = interval;
		callback_table[callback_num].repeats = repeats;
		callback_table[callback_num].context = context;

		callback_num++;

		return callback_table[callback_num];
	}

	//TODO expand table instead of clearing it
	clear_table();
	//try adding the callback again now that we know the table has room
	return add_callback(callback, interval, repeats, context);
}

void remove_callback(timer_callback callback) {
	//find this callback in callback table
	for (int i = 0; i < callback_num; i++) {
		if (callback_table[callback_num].callback == callback.callback) {
			memset(&callback_table[i], 0, sizeof(timer_callback));
		}
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void handle_tick(uint32_t tick) {
	//look through every callback and see if we should fire
	for (int i = 0; i < callback_num; i++) {
		//decrement time left
		callback_table[i].time_left -= 1;

		//if it's time to fire, do so
		if (callback_table[i].time_left <= 0) {
			//reset for next firing
			callback_table[i].time_left = callback_table[i].interval;

			void(*callback_func)(void*) = (void(*)(void*))callback_table[i].callback;
			callback_func(callback_table[i].context);

			//if we only fire once, trash this callback
			if (!callback_table[i].repeats) {
				remove_callback(callback_table[i]);
			}
		}
	}
}
#pragma GCC diagnostic pop

void sleep(uint32_t ms) {
	uint32_t end = time() + ms;
    extern task_t* current_task;
    current_task->wake_timestamp = end;
    sys_yield(PIT_WAIT);
}
