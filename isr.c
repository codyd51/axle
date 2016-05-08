#include "common.h"
#include "isr.h"
#include "kernel.h"

//gets called from ASM interrupt handler stub
void isr_handler(registers_t regs) {
	terminal_settextcolor(COLOR_MAGENTA);
	kprintf("recieved interrupt: %d err code: %d", regs.int_no, regs.err_code);
}

isr_t interrupt_handlers[256];

void register_interrupt_handler(u8int n, isr_t handler) {
	interrupt_handlers[n] = handler;
}

//gets called from ASM interrupt handler stub
void irq_handler(registers_t regs) {
	//sends an EOI (end of interrupt) signal to PICs
	//if this interrupt involved the slave
	if (regs.int_no >= 40) {
		//send reset signal to slave
		outb(0xA0, 0x20);
	}
	//send reset signal to master
	outb(0x20, 0x20);

	if (interrupt_handlers[regs.int_no] != 0) {
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);
	}
}
