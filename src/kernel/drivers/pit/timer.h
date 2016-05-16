#ifndef TIMER_H
#define TIMER_H

#include <std/common.h>

void init_timer(u32int frequency);
u32int tickCount();
void sleep(u32int ms);

#endif
