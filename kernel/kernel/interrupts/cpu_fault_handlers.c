#include "cpu_fault_handlers.h"

#include <stdbool.h>
#include <std/printf.h>

#include <kernel/assert.h>

static void common_halt(register_state_t* regs, bool recoverable) {
    printf("|- RIP = 0x%08x -|\n", regs->return_rip);
    printf("|- RSP = 0x%08x -|\n", regs->return_rsp);
    printf("|- Error code = 0x%08x -|\n", regs->err_code);
	assert(0, "CPU exception");
	task_assert(false, "test", regs);
}

void interrupt_handle_divide_by_zero(register_state_t* regs) {
	printf_err("Attempted division by zero");
	common_halt(regs, false);
}

void interrupt_handle_bound_range_exceeded(register_state_t* regs) {
	printf_err("Bound range exception");
	common_halt(regs, false);
}

void interrupt_handle_invalid_opcode(register_state_t* regs) {
	printf_err("Invalid opcode encountered");
	common_halt(regs, false);
}

void interrupt_handle_device_not_available(register_state_t* regs) {
	printf_err("Device not available");
	common_halt(regs, false);
}

void interrupt_handle_double_fault(register_state_t* regs) {
	printf_err("=======================");
	printf_err("Caught double fault!");
	printf_err("=======================");
	common_halt(regs, false);
}

void interrupt_handle_invalid_tss(register_state_t* regs) {
	printf_err("Invalid TSS section!");
	common_halt(regs, false);
}

void interrupt_handle_segment_not_present(register_state_t* regs) {
	printf_err("Segment not present");
	common_halt(regs, false);
}

void interrupt_handle_stack_segment_fault(register_state_t* regs) {
	printf_err("Stack segment fault");
	common_halt(regs, false);
}

void interrupt_handle_general_protection_fault(register_state_t* regs) {
	printf_err("General protection fault");
	common_halt(regs, false);
}

void interrupt_handle_floating_point_exception(register_state_t* regs) {
	printf_err("Floating point exception");
	common_halt(regs, false);
}

void interrupt_handle_alignment_check(register_state_t* regs) {
	printf_err("Alignment check");
	common_halt(regs, false);
}

void interrupt_handle_machine_check(register_state_t* regs) {
	printf_err("Machine check");
	common_halt(regs, false);
}

void interrupt_handle_virtualization_exception(register_state_t* regs) {
	printf_err("Virtualization exception");
	common_halt(regs, false);
}
