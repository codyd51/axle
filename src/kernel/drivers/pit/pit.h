#ifndef PIT_H
#define PIT_H

#include <std/common.h>

#define PIT_TICK_GRANULARITY_1MS  1000
#define PIT_TICK_GRANULARITY_5MS  200
#define PIT_TICK_GRANULARITY_10MS 100
#define PIT_TICK_GRANULARITY_20MS 50
#define PIT_TICK_GRANULARITY_50MS 20

void pit_timer_init(uint32_t frequency);
uint32_t pit_clock();

//explicitly deprecated
uint32_t tick_count();

#endif
