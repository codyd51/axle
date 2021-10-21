#include "pit.h"
#include <kernel/kernel.h>
#include <kernel/assert.h>
#include <std/math.h>
#include <std/common.h>
#include <std/printf.h>
#include <kernel/boot_info.h>
#include <kernel/util/amc/amc.h>

//channel 0 used for generating IRQ0
#define PIT_PORT_CHANNEL0 0x40
//channel 1 used for refreshing DRAM
#define PIT_PORT_CHANNEL1 0x41
//channel 2 used for controlling speaker
#define PIT_PORT_CHANNEL2 0x42
//command port for controlling PIT
#define PIT_PORT_COMMAND  0x43

static volatile uint32_t tick = 0;

static int tick_callback(register_state_t* regs) {
	tick++;
	// Wake sleeping services befores ending EOI, or else we 
	// might get interrupted by another tick while the AMC spinlock is held
	amc_wake_sleeping_services();
	pic_signal_end_of_interrupt(regs->int_no);
	task_switch_if_quantum_expired();
	return 0;
}

uint32_t pit_clock() {
	return tick;
}

uint32_t tick_count() {
    return pit_clock();
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
	
	boot_info_get()->ms_per_pit_tick = 1000 / frequency;
}

uint32_t ms_since_boot(void) {
	return tick * boot_info_get()->ms_per_pit_tick;
}
