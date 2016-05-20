#ifndef TIMER_H
#define TIMER_H

#include <std/std.h>

#define MAX_CALLBACKS 100

typedef struct {
	void* callback;
	uint32_t interval;
	uint32_t time_left;
	bool repeats;
	void* context;
} timer_callback;

timer_callback add_callback(void* callback, int interval, bool repeats, void* context);
void remove_callback_at_index(int index);

#endif
