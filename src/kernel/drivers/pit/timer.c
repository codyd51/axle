#include "timer.h"
#include <kernel/util/interrupts/isr.h>
#include <kernel/kernel.h>

#include <std/common.h>

uint32_t tick = 0;

static void timer_callback(registers_t regs) {
	tick++;
}

uint32_t tickCount() {
	return tick;
}

void init_timer(uint32_t frequency) {
	terminal_settextcolor(COLOR_LIGHT_GREY);
	printf("init timer called\n");
	
	//firstly, register our timer callback
	register_interrupt_handler(IRQ0, &timer_callback);

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

void sleep(uint32_t ms) {
	uint32_t end = tick + ms;
	while (tick < end) {}
}
