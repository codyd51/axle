#ifndef STD_TIMER_H
#define STD_TIMER_H

#include "std_base.h"
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

#define MAX_CALLBACKS 100

typedef struct {
	void* callback;
	uint32_t interval;
	uint32_t time_left;
	bool repeats;
	void* context;
} timer_callback;

STDAPI timer_callback add_callback(void* callback, int interval, bool repeats, void* context);
STDAPI void remove_callback_at_index(int index);

__END_DECLS

#endif // STD_TIMER_H
