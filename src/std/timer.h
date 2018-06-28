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

STDAPI void timer_init();

STDAPI timer_callback_t* timer_callback_register(void* callback, int interval, bool repeats, void* context);
STDAPI void timer_callback_remove(timer_callback_t* callback);
STDAPI void timer_callback_deliver_immediately(timer_callback_t* callback);

STDAPI void sleep(uint32_t ms);

__END_DECLS

#endif // STD_TIMER_H
