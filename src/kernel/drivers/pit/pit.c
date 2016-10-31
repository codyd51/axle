#include "pit.h"
#include <kernel/util/interrupts/isr.h>
#include <kernel/kernel.h>
#include <std/math.h>
#include <std/common.h>
#include <std/printf.h>

static volatile uint32_t tick = 0;

//defined in timer.c
//inform that a tick has occured
extern void handle_tick(uint32_t tick);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void tick_callback(registers_t regs) {
	tick++;

	handle_tick(tick);
}
#pragma GCC diagnostic pop

uint32_t tick_count() {
	return tick;
}

void pit_install(uint32_t frequency) {
	printf_info("Initializing PIT timer...");
	printf("\e[7;");

	//firstly, register our timer callback
	register_interrupt_handler(IRQ0, &tick_callback);

	//value we need to send to PIC is value to divide it's input clock
	//(1193180 Hz) by, to get desired frequency
	//divisor *must* be small enough to fit into 16 bytes
	uint32_t divisor = 1193180 / frequency;

	//send command byte
	outb(0x43, 0x36);

	//divisor has to be sent byte-wise, so split here into upper/lower bytes
	uint8_t l = (uint8_t )(divisor & 0xFF);
	uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

	//send frequency divisor
	outb(0x40, l);
	outb(0x40, h);
}
