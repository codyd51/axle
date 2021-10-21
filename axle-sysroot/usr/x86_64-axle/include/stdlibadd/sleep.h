#ifndef SLEEP_H 
#define SLEEP_H

#include <stdint.h>

unsigned sleep(unsigned int seconds);
unsigned usleep(unsigned int ms);

uint32_t ms_since_boot();

#endif