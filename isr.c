#include "common.h"
#include "isr.h"
#include "kernel.h"

void handle_page_fault(registers_t regs) {
	//TODO cr2 holds address that caused the fault
	printf_err("Encountered page fault. Info follows");

	if (regs.err_code & 0x000F) printf_err("Page was present");
	else printf_err("Page was not present");
	
	if (regs.err_code & 0x00F0) printf_err("Operation was a write");
	else printf_err("Operation was a read");

	if (regs.err_code & 0x0F00) printf_err("User mode");
	else printf_err("Supervisor mode");

	if (regs.err_code & 0xF000) printf_err("Faulted during instruction fetch");

	printf_info("Halting execution");
	do {} while (1);
}

//gets called from ASM interrupt handler stub
void isr_handler(registers_t regs) {
	printf_info("recieved interrupt: %d err code: %d", regs.int_no, regs.err_code);

	if (regs.int_no == 14) {
		handle_page_fault(regs);
	}
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
