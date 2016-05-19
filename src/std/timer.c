#include "timer.h"
#include <limits.h>

int callback_num;
static timer_callback callback_table[MAX_CALLBACKS];

int add_callback(void* callback, uint32_t interval, bool repeats, void* context) {
	//only add callback if we have room
	if (callback_num + 1 < MAX_CALLBACKS) {
		callback_table[callback_num].callback = callback;
		callback_table[callback_num].interval = interval;
		callback_table[callback_num].repeats = repeats;
		callback_table[callback_num].context = context;	

		callback_num++;
	}
	else return -1;

	//we iterated callback_num in the above block,
	//so subtract 1 before returning the callback index
	return callback_num - 1;
}

void remove_callback_at_index(int index) {
	timer_callback callback = callback_table[index];
	//set function pointer to null so it can't fire again
	//also set desired interval to int_max so it never attempts to fire anyways
	callback.callback = NULL;
	callback.interval = INT_MAX;
}

void handle_tick(uint32_t tick) {
	//look through every callback and see if we should fire
	for (int i = 0; i < callback_num; i++) {
		timer_callback callback = callback_table[i];
		
		//interval is 1 / frequency (interval)
		if (tick % (1 / callback.interval) == 0) {
			void(*callback_func)(void*) = callback.callback;
			callback_func(callback.context);

			//if this timer is only supposed to be fired once, set the function pointer to null
			//so it can't fire again
			//also remove the interval so this never gets hit regardless
			if (!callback.repeats) {
				remove_callback_at_index(i);
			}
		}
	}
}
