#ifndef TIMER_H
#define TIMER_H

#include <std/common.h>

void init_timer(uint32_t frequency);
uint32_t tickCount();
void sleep(uint32_t ms);

#endif
