#include "pit.h"
#include <kernel/interrupts/interrupts.h>
#include <kernel/kernel.h>
#include <kernel/assert.h>
#include <std/math.h>
#include <std/common.h>
#include <std/printf.h>

//channel 0 used for generating IRQ0
#define PIT_PORT_CHANNEL0 0x40
//channel 1 used for refreshing DRAM
#define PIT_PORT_CHANNEL1 0x41
//channel 2 used for controlling speaker
#define PIT_PORT_CHANNEL2 0x42
//command port for controlling PIT
#define PIT_PORT_COMMAND  0x43

static volatile uint32_t tick = 0;

//defined in timer.c
//inform that a tick has occured
extern void handle_tick(uint32_t tick);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void tick_callback(registers_t regs) {
	tick++;

	//handle_tick(tick);
}
#pragma GCC diagnostic pop

uint32_t pit_clock() {
	return tick;
}

uint32_t tick_count() {
    NotImplemented();
}

void pit_timer_init(uint32_t frequency) {
	printf_info("Initializing PIT timer...");

	//firstly, register our timer callback
	interrupt_setup_callback(INT_VECOR_IRQ0, &tick_callback);

	//value we need to send to PIC is value to divide it's input clock
	//(1193180 Hz) by, to get desired frequency
	//divisor *must* be small enough to fit into 16 bytes
	uint32_t divisor = 1193180 / frequency;

	//send command byte
	outb(PIT_PORT_COMMAND, 0x36);

	//divisor has to be sent byte-wise, so split here into upper/lower bytes
	uint8_t l = (uint8_t )(divisor & 0xFF);
	uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

	//send frequency divisor
	outb(PIT_PORT_CHANNEL0, l);
	outb(PIT_PORT_CHANNEL0, h);
}
