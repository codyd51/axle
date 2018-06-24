#ifndef STD_TIMER_H
#define STD_TIMER_H

#include "std_base.h"
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

#define MAX_CALLBACKS 100

typedef struct {
	void* func;
	uint32_t interval;
	uint32_t time_left;
	bool repeats;
	void* context;
} timer_callback_t;

STDAPI timer_callback_t* timer_add_callback(void* callback, int interval, bool repeats, void* context);
STDAPI void timer_remove_callback(timer_callback_t* callback);
STDAPI void timer_deliver_immediately(timer_callback_t* callback);

STDAPI void sleep(uint32_t ms);

//friend function for pit.c
void _timer_handle_pit_tick();

__END_DECLS

#endif // STD_TIMER_H
