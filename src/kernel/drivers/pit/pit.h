#ifndef PIT_H 
#define PIT_H

#include <std/common.h>

void pit_install(uint32_t frequency);
uint32_t tick_count();
void sleep(uint32_t ms);

#endif
