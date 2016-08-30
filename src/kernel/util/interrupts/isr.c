#include <std/common.h>
#include "isr.h"
#include <kernel/kernel.h>

void halt_execution() {
	//disable interrupts so this never ends
	kernel_begin_critical();

	//enter infinite loop
	printf_err("Halting execution");
	do {} while (1);
}

void print_regs(registers_t regs) {
	printf("eax: %x		ecx: %x		edx: %x		ebx: %x\n", regs.eax, regs.ecx, regs.edx, regs.ebx);
	printf("esp: %x		ebp: %x 	esi: %x		edi: %x\n", regs.esp, regs.ebp, regs.esi, regs.edi);
	printf("eip: %x		cs:  %x		ds:  %x		eflags: %x\n", regs.eip, regs.cs, regs.ds, regs.eflags);
}

void common_halt(registers_t regs, bool recoverable) {
	//print out register info for debugging
	print_regs(regs);

	if (!recoverable) {
		//stop everything so we don't triple fault
		halt_execution();
	}
}

void print_selector_error_code(uint32_t err_code) {
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
	common_halt(regs, false);
}

void handle_bound_range_exceeded(registers_t regs) {
	printf_err("Bound range exception");
	common_halt(regs, false);
}

void handle_invalid_opcode(registers_t regs) {
	printf_err("Invalid opcode encountered");
	common_halt(regs, false);
}

void handle_device_not_available(registers_t regs) {
	printf_err("Device not available");
	common_halt(regs, false);
}

void handle_double_fault(registers_t regs) {
	printf_err("=======================");
	printf_err("Caught double fault!");
	printf_err("=======================");
	common_halt(regs, false);
}

void handle_invalid_tss(registers_t regs) {
	printf_err("Invalid TSS section!");
	print_selector_error_code(regs.err_code);
	common_halt(regs, false);
}

void handle_segment_not_present(registers_t regs) {
	printf_err("Segment not present");
	print_selector_error_code(regs.err_code);
	common_halt(regs, false);
}

void handle_stack_segment_fault(registers_t regs) {
	printf_err("Stack segment fault");
	print_selector_error_code(regs.err_code);
	common_halt(regs, false);
}

void handle_general_protection_fault(registers_t regs) {
	printf_err("General protection fault");
	print_selector_error_code(regs.err_code);
	common_halt(regs, false);
}

void handle_floating_point_exception(registers_t regs) {
	printf_err("Floating point exception");
	common_halt(regs, false);
}

void handle_alignment_check(registers_t regs) {
	printf_err("Alignment check");
	common_halt(regs, false);
}

void handle_machine_check(registers_t regs) {
	printf_err("Machine check");
	common_halt(regs, false);
}

void handle_virtualization_exception(registers_t regs) {
	printf_err("Virtualization exception");
	common_halt(regs, false);
}

isr_t interrupt_handlers[256];

void isr_install_default() {
	register_interrupt_handler(0, &handle_divide_by_zero);
	register_interrupt_handler(5, &handle_bound_range_exceeded);
	register_interrupt_handler(6, &handle_invalid_opcode);
	register_interrupt_handler(7, &handle_device_not_available);
	register_interrupt_handler(8, &handle_double_fault);
	register_interrupt_handler(10, &handle_invalid_tss);
	register_interrupt_handler(11, &handle_segment_not_present);
	register_interrupt_handler(12, &handle_stack_segment_fault);
	register_interrupt_handler(13, &handle_general_protection_fault);
	register_interrupt_handler(16, &handle_floating_point_exception);
	register_interrupt_handler(19, &handle_floating_point_exception);
	register_interrupt_handler(17, &handle_alignment_check);
	register_interrupt_handler(18, &handle_machine_check);
	register_interrupt_handler(20, &handle_virtualization_exception);
}

//gets called from ASM interrupt handler stub
void isr_handler(registers_t regs) {
	//uint8_t int_no = regs.int_no & 0xFF;
	uint8_t int_no = regs.int_no;
	if (interrupt_handlers[int_no] != 0) {
		isr_t handler = interrupt_handlers[int_no];
		handler(regs);
	}
	else {
		printf_err("Unhandled interrupt: %x", int_no);
		common_halt(regs, true);
	}
}

void register_interrupt_handler(uint8_t n, isr_t handler) {
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
		outb(PIC2_PORT_A, PIC_ACK);
	}
	outb(PIC1_PORT_A, PIC_ACK);
}

//gets called from ASM interrupt handler stub
void irq_handler(registers_t regs) {
	pic_acknowledge(regs.int_no);
	if (interrupt_handlers[regs.int_no] != 0) {
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);
	}
}
