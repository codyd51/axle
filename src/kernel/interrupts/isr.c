#include "isr.h"
#include "idt_structures.h"

#include <std/common.h>

#include <kernel/kernel.h>
#include <kernel/util/multitasking/tasks/task.h>
#include <kernel/assert.h>

/*
void halt_execution() {
	//kill this task
	printf_info("PID %d encountered unrecoverable fault, killing...", getpid());
	if (getpid() == 1 || getpid() == -1) {
		printf_info("kernel died!");
		while (1) {}
	}
	_kill();
}

void print_regs(registers_t regs) {
    printf("\n======================    registers    ========================\n");
	printf("eax: %x		ecx: %x		edx: %x		ebx: %x\n", regs.eax, regs.ecx, regs.edx, regs.ebx);
	printf("esp: %x		ebp: %x 	esi: %x		edi: %x\n", regs.esp, regs.ebp, regs.esi, regs.edi);
	printf("eip: %x		int: %x		err: %x		cs:  %x\n", regs.eip, regs.int_no, regs.err_code, regs.cs);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsequence-point"
//TODO: figure out undefinedness of "esp--"
void dump_stack(uint32_t* esp) {
	printk("dump @ %x\n", esp);
	
	//jump back 16 bytes
	//4 bytes in a uint32_t
	//16 / 4 == 4
	esp -= 4;

	for (int i = 0; i < 8; i++) {
		uint32_t* current_base = esp;
		printk("[%x] ", esp);
		//print each byte in word
		for (int j = 0; j < 4; j++) {
			printk("%x ", *(esp++));
		}

#define GET_BYTE(number, byte_idx) (number >> (8*byte_idx)) & 0xff
		//we want to print out every byte of the 4 words we just printed out
		for (int i = 0; i < 4; i++) {
			uint32_t* ptr = current_base;
			//for each byte in current word
			for (size_t j = 0; j < sizeof(uint32_t); j++) {
				uint8_t val = GET_BYTE(*ptr, j);
				if (isalnum(val)) {
					printk("%c", val);
				}
				else {
					printk(".");
				}
			}
			ptr++;
		}
		printk("\n");
	}
}
#pragma GCC diagnostic pop

void common_halt(registers_t UNUSED(regs), bool recoverable) {
	//print out register info for debugging
	//print_regs(regs);
	printf("skipping reg output\n");

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
*/

void register_interrupt_handler(uint8_t n, isr_t handler) {
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
	/*
	uint8_t int_no = regs->int_no;
    pic_acknowledge(int_no);

	int ret = 0;
	if (interrupt_handlers[int_no] != 0) {
		//syscalls get pointer to register state
		//also, return value is syscall return
		if (int_no == 0x80) {
			typedef int (*isr_reg_t)(registers_t*);
			isr_reg_t handler = ((isr_reg_t*)interrupt_handlers)[int_no];
			//set ret val
			ret = handler(regs);
		}
		else {
			isr_t handler = interrupt_handlers[int_no];
			handler(*regs);
		}
	}
	else {
		printf_err("Unhandled ISR: %d", int_no);
	}
	return ret;
	*/
	printf("Received interrupt: %d\n", regs.int_no);
}

/*
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