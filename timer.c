#include "timer.h"
#include "isr.h"
#include "kernel.h"

#include "common.h"

u32int tick = 0;

static void timer_callback(registers_t regs) {
	tick++;
}

u32int tickCount() {
	return tick;
}

void init_timer(u32int frequency) {
	terminal_settextcolor(COLOR_LIGHT_GREY);
	printf("init timer called\n");
	
	//firstly, register our timer callback
	register_interrupt_handler(IRQ0, &timer_callback);

	//value we need to send to PIC is value to divide it's input clock
	//(1193180 Hz) by, to get desired frequency
	//divisor *must* be small enough to fit into 16 bytes
	u32int divisor = 1193180 / frequency;

	//send command byte
	outb(0x43, 0x36);

	//divisor has to be sent byte-wise, so split here into upper/lower bytes
	u8int l = (u8int)(divisor & 0xFF);
	u8int h = (u8int)((divisor >> 8) & 0xFF);

	//send frequency divisor
	outb(0x40, l);
	outb(0x40, h);
}

void sleep(u32int ms) {
	u32int end = tick + ms;
	while (tick < end) {}
}
