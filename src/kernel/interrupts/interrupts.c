#include "interrupts.h"
#include "idt_structures.h"
#include "idt.h"
#include "cpu_fault_handlers.h"

#include <std/common.h>

#include <kernel/kernel.h>
#include <kernel/multitasking//tasks/task.h>
#include <kernel/assert.h>

static int_callback_t interrupt_handlers[256] = {0};

#define IRQ0  32
#define IRQ1  33
#define IRQ2  34
#define IRQ3  35
#define IRQ4  36
#define IRQ5  37
#define IRQ6  38
#define IRQ7  39
#define IRQ8  40
#define IRQ9  41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

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

//gets called from ASM interrupt handler stub
int isr_receive(register_state_t* regs) {
	int ret = 0;
	uint8_t int_no = regs->int_no;
	if (interrupt_handlers[int_no] != 0) {
		int_callback_t handler = interrupt_handlers[int_no];
		ret = handler(regs);
	}
	else {
		printf("Unhandled interrupt: %d\n", int_no);
	}
	return ret;
}

//gets called from ASM interrupt handler stub
void irq_receive(register_state_t* regs) {
	uint8_t int_no = regs->int_no;

	int ret = 0;
	if (interrupt_handlers[int_no] != 0) {
		int_callback_t handler = interrupt_handlers[int_no];
		ret = handler(regs);
	}
	else {
		printf("Unhandled IRQ: %d\n", int_no);
	}

    pic_signal_end_of_interrupt(int_no);

	extern uint32_t* _current_task_small;
	if (int_no == 0x20 && _current_task_small != NULL) {
		void task_switch();
		task_switch();
	}

	return ret;
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
