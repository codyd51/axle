#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <std/common.h>

#include "idt_structures.h"
#include "idt.h"

typedef int (*int_callback_t)(register_state_t);

//sets up IDT,
//and registers default interrupt handlers
void interrupt_init(void);

//request a callback to be invoked when interrupt vector `interrupt_num` is
//processed by the CPU
void interrupt_setup_callback(uint8_t interrupt_num, int_callback_t callback);

#endif
