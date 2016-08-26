#ifndef MULT_UTIL_H
#define MULT_UTIL_H

#include <kernel/util/interrupts/isr.h>

//causes current process' stack to be forcibly moved to different location
void move_stack(void* new_stack_start, uint32_t size);

#endif
