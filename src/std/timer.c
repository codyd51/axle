#include "timer.h"
#include <limits.h>

int callback_num;
static timer_callback callback_table[MAX_CALLBACKS];

int add_callback(void* callback, int interval, bool repeats, void* context) {
	//only add callback if we have room
	if (callback_num + 1 < MAX_CALLBACKS) {
		callback_table[callback_num].callback = callback;
		callback_table[callback_num].interval = interval;
		callback_table[callback_num].time_left = interval;
		callback_table[callback_num].repeats = repeats;
		callback_table[callback_num].context = context;	

		callback_num++;

		//we iterated callback_num, so subtract 1 before returning index
		return callback_num - 1;
	}
	
	return -1;
}

void remove_callback_at_index(int index) {
	//set function pointer to null so it can't fire again
	//also set desired interval to int_max so it never attempts to fire anyways
	//callback_table[index].callback = NULL;
	callback_table[index].interval = INT_MAX;
}

void handle_tick(uint32_t tick) {
	//look through every callback and see if we should fire
	for (int i = 0; i < callback_num; i++) {
		//decrement time left
		callback_table[i].time_left -= 1;

		//if it's time to fire, do so
		if (callback_table[i].time_left <= 0) {
			//reset for next firing
			callback_table[i].time_left = callback_table[i].interval;

			void(*callback_func)(void*) = callback_table[i].callback;
			callback_func(callback_table[i].context);

			//if we only fire once, trash this callback
			if (!callback_table[i].repeats) {
				remove_callback_at_index(i);
			}
		}
	}
}
