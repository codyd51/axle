#include "interrupts.h"
#include "idt_structures.h"
#include "idt.h"
#include "interrupt_handlers.h"

#include <std/common.h>

#include <kernel/kernel.h>
#include <kernel/util/multitasking/tasks/task.h>
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
/*
//gets called from ASM interrupt handler stub
void irq_handler(registers_t regs) {
	pic_acknowledge(regs.int_no);
	if (interrupt_handlers[regs.int_no] != 0) {
		isr_t handler = interrupt_handlers[regs.int_no];
		handler(regs);

		//unblock any tasks waiting for this IRQ
		task_t* tmp = task_list();
		while (tmp != NULL) {
			if (tmp->state == IRQ_WAIT) {
				uint32_t requested = (uint32_t)tmp->block_context;
				if (requested == regs.int_no) {
					tmp->irq_satisfied = true;
					update_blocked_tasks();
				}
			}
			tmp = tmp->next;
		}
	}
	else printf_dbg("unhandled IRQ %d", regs.int_no);
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
int isr_receive(register_state_t regs) {
	uint8_t int_no = regs.int_no;
    pic_signal_end_of_interrupt(int_no);

	int ret = 0;
	if (interrupt_handlers[int_no] != 0) {
		int_callback_t handler = interrupt_handlers[int_no];
		ret = handler(regs);
	}
	else {
		printf("Unhandled interrupt: %d\n", int_no);
	}
	return ret;
}

static void interrupt_register_defaults(void) {
    interrupt_register_handler(0, &interrupt_handle_divide_by_zero);
	interrupt_register_handler(5, &interrupt_handle_bound_range_exceeded);
	interrupt_register_handler(6, &interrupt_handle_invalid_opcode);
	interrupt_register_handler(7, &interrupt_handle_device_not_available);
	interrupt_register_handler(8, &interrupt_handle_double_fault);
	interrupt_register_handler(10, &interrupt_handle_invalid_tss);
	interrupt_register_handler(11, &interrupt_handle_segment_not_present);
	interrupt_register_handler(12, &interrupt_handle_stack_segment_fault);
	interrupt_register_handler(13, &interrupt_handle_general_protection_fault);
	interrupt_register_handler(16, &interrupt_handle_floating_point_exception);
	interrupt_register_handler(19, &interrupt_handle_floating_point_exception);
	interrupt_register_handler(17, &interrupt_handle_alignment_check);
	interrupt_register_handler(18, &interrupt_handle_machine_check);
	interrupt_register_handler(20, &interrupt_handle_virtualization_exception);
}

void interrupt_init(void) {
    idt_init();
    interrupt_register_defaults();
}

void interrupt_register_handler(uint8_t interrupt_num, int_callback_t callback) {
    if (interrupt_handlers[interrupt_num] != 0) {
        assert("Tried to overwrite handler for interrupt %d", interrupt_num);
    }
    interrupt_handlers[interrupt_num] = callback;
}
