#include "common.h"
#include "isr.h"
#include "kernel.h"

void halt_execution() {
	//disable interrupts so this never ends
	asm volatile("cli");
	//enter infinite loop
	printf_err("Halting execution");
	do {} while (1);
}

void print_selector_error_code(u32int err_code) {
	printf_err("Selector error code %x interpreted below", err_code);

	if (err_code & 0xF) printf_err("Exception originated outside processor");
	else printf_err("Exception occurred inside processor");

	if (err_code & 0x00) printf_err("Selector index references descriptor in GDT");
	else if (err_code & 0x01) printf_err("Selector index references descriptor in IDT");
	else if (err_code & 0x10) printf_err("Selector index references descriptor in LDT");
	else if (err_code & 0x11) printf_err("Selector index references descruptor in IDT");

	printf_info("Selector index follows");
	//TODO print index
	//index is bits 3 to 15 (13 bits)
}

void handle_divide_by_zero(registers_t regs) {
	printf_err("Attempted division by zero");
	halt_execution();
}

void handle_bound_range_exceeded(registers_t regs) {
	printf_err("Bound range exception");
	halt_execution();
}

void handle_invalid_opcode(registers_t regs) {
	printf_err("Invalid opcode encountered");
	halt_execution();
}

void handle_device_not_available(registers_t regs) {
	printf_err("Device not available");
	halt_execution();
}

void handle_double_fault(registers_t regs) {
	printf_err("=======================");
	printf_err("Caught double fault!");
	printf_err("=======================");
	halt_execution();
}

void handle_invalid_tss(registers_t regs) {
	printf_err("Invalid TSS section!");
	print_selector_error_code(regs.err_code);
	halt_execution();
}

void handle_segment_not_present(registers_t regs) {
	printf_err("Segment not present");
	print_selector_error_code(regs.err_code);
	halt_execution();
}

void handle_stack_segment_fault(registers_t regs) {
	printf_err("Stack segment fault");
	print_selector_error_code(regs.err_code);
	halt_execution();
}

void handle_general_protection_fault(registers_t regs) {
	printf_err("General protection fault");
	print_selector_error_code(regs.err_code);
	halt_execution();
}

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

	halt_execution();
}

void handle_floating_point_exception(registers_t regs) {
	printf_err("Floating point exception");
	halt_execution();
}

void handle_alignment_check(registers_t regs) {
	printf_err("Alignment check");
	halt_execution();
}

void handle_machine_check(registers_t regs) {
	printf_err("Machine check");
	halt_execution();
}

void handle_virtualization_exception(registers_t regs) {
	printf_err("Virtualization exception");
	halt_execution();
}

//gets called from ASM interrupt handler stub
void isr_handler(registers_t regs) {
	printf_info("recieved interrupt: %d err code: %d", regs.int_no, regs.err_code);

	switch (regs.int_no) {
		case 0:
			handle_divide_by_zero(regs);
			break;
		case 5:
			handle_bound_range_exceeded(regs);
			break;
		case 6:
			handle_invalid_opcode(regs);
			break;
		case 7:
			handle_device_not_available(regs);
			break;
		case 8:
			handle_double_fault(regs);
			break;
		case 10:
			handle_invalid_tss(regs);
			break;
		case 11:
			handle_segment_not_present(regs);
			break;
		case 12:
			handle_stack_segment_fault(regs);
			break;
		case 13:
			handle_general_protection_fault(regs);
			break;
		case 14:
			handle_page_fault(regs);
			break;
		case 16:
		case 19:
			handle_floating_point_exception(regs);
			break;
		case 17:
			handle_alignment_check(regs);
			break;
		case 18:
			handle_machine_check(regs);
			break;
		case 20:
			handle_virtualization_exception(regs);
			break;
		case 128:
			printf_info("Got syscall");
			break;
	}
}

isr_t interrupt_handlers[256];

void register_interrupt_handler(u8int n, isr_t handler) {
	interrupt_handlers[n] = handler;
}

#define PIC1_PORT_A 0x20
#define PIC2_PORT_A 0xA0

#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28
#define PIC2_END_INTERRUPT PIC2_START_INTERRUPT + 7

#define PIC_ACK 0x20

void pic_acknowledge(unsigned int interrupt) {
	if (interrupt < PIC1_START_INTERRUPT || interrupt > PIC2_END_INTERRUPT) {
		return;
	}

	if (interrupt < PIC2_START_INTERRUPT) {
		outb(PIC1_PORT_A, PIC_ACK);
	}
	else outb(PIC2_PORT_A, PIC_ACK);
}

//gets called from ASM interrupt handler stub
void irq_handler(registers_t regs) {
	if (interrupt_handlers[regs.int_no] != 0) {
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);
	}
	pic_acknowledge(regs.int_no);
}
