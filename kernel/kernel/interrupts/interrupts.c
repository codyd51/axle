#include "interrupts.h"
#include "idt_structures.h"
#include "idt.h"
#include "cpu_fault_handlers.h"

#include <std/common.h>

#include <kernel/kernel.h>
#include <kernel/assert.h>

static int_callback_t interrupt_handlers[256] = {0};

/*
void print_regs(registers_t regs) {
    printf("\n======================    registers    ========================\n");
	printf("eax: %x		ecx: %x		edx: %x		ebx: %x\n", regs.eax, regs.ecx, regs.edx, regs.ebx);
	printf("esp: %x		ebp: %x 	esi: %x		edi: %x\n", regs.esp, regs.ebp, regs.esi, regs.edi);
	printf("eip: %x		int: %x		err: %x		cs:  %x\n", regs.eip, regs.int_no, regs.err_code, regs.cs);
}
*/

void register_interrupt_handler(uint8_t n, int_callback_t handler) {
	NotImplemented();
}

void common_halt(void) {
	NotImplemented();
}

void irq_handler(void) {
	NotImplemented();
}

void dump_stack(uint32_t* mem) {
	NotImplemented();
}

// Called from asm interrupt_trampoline
void interrupt_handle(register_state_t* regs) {
	uint8_t int_no = regs->int_no;
	bool is_external = (bool)regs->is_external_interrupt;
	//printf("interrupt_handle(%d, is_external %d err_code %d)\n", regs->int_no, regs->is_external_interrupt, regs->err_code);

	if (interrupt_handlers[int_no] != 0) {
		int_callback_t handler = interrupt_handlers[int_no];
		handler(regs);
	}
	else {
		printf("Unhandled IRQ: %d\n", int_no);
		if (is_external) {
			pic_signal_end_of_interrupt(int_no);
		}
	}

	// If there is an adi driver for this IRQ, the EOI will be sent by adi
	// Also, the PIT EOI will be sent by the PIT driver
	// TODO(PT): Is this needed or is the unhandled interrupt check above enough?
	if (is_external && !adi_services_interrupt(int_no) && int_no != INT_VECOR_IRQ0) {
		pic_signal_end_of_interrupt(int_no);
	}
}


static void interrupt_setup_error_callbacks(void) {
    interrupt_setup_callback(0, &interrupt_handle_divide_by_zero);
	interrupt_setup_callback(5, &interrupt_handle_bound_range_exceeded);
	interrupt_setup_callback(6, &interrupt_handle_invalid_opcode);
	interrupt_setup_callback(7, &interrupt_handle_device_not_available);
	interrupt_setup_callback(8, &interrupt_handle_double_fault);
	interrupt_setup_callback(10, &interrupt_handle_invalid_tss);
	interrupt_setup_callback(11, &interrupt_handle_segment_not_present);
	interrupt_setup_callback(12, &interrupt_handle_stack_segment_fault);
	interrupt_setup_callback(13, &interrupt_handle_general_protection_fault);
	interrupt_setup_callback(16, &interrupt_handle_floating_point_exception);
	interrupt_setup_callback(19, &interrupt_handle_floating_point_exception);
	interrupt_setup_callback(17, &interrupt_handle_alignment_check);
	interrupt_setup_callback(18, &interrupt_handle_machine_check);
	interrupt_setup_callback(20, &interrupt_handle_virtualization_exception);
}

void interrupt_init(void) {
    idt_init();
    interrupt_setup_error_callbacks();
    //now that we've set everything up, enable interrupts
    asm("sti");
}

void interrupt_setup_callback(uint8_t interrupt_num, int_callback_t callback) {
    if (interrupt_handlers[interrupt_num] != 0) {
        assert("Tried to overwrite handler for interrupt %d", interrupt_num);
    }
    interrupt_handlers[interrupt_num] = callback;
}
