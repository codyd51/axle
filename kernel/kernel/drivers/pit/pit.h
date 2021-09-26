#ifndef PIT_H
#define PIT_H

#include <std/common.h>
#include <kernel/interrupts/interrupts.h>

#define PIT_INT_VECTOR INT_VECOR_IRQ0

#define PIT_TICK_GRANULARITY_1MS  1000
#define PIT_TICK_GRANULARITY_5MS  200
#define PIT_TICK_GRANULARITY_10MS 100
#define PIT_TICK_GRANULARITY_20MS 50
#define PIT_TICK_GRANULARITY_50MS 20
#define PIT_TICK_GRANULARITY_100MS 10
#define PIT_TICK_GRANULARITY_1000MS 1

void pit_timer_init(uint32_t frequency);

uint32_t pit_clock();
uint32_t tick_count();

uint32_t ms_since_boot(void);

#endif
