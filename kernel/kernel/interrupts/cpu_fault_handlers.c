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
	printf("Attempted division by zero\n");
	common_halt(regs, false);
}

void interrupt_handle_bound_range_exceeded(register_state_t* regs) {
	printf("Bound range exception\n");
	common_halt(regs, false);
}

void interrupt_handle_invalid_opcode(register_state_t* regs) {
	printf("Invalid opcode encountered\n");
	common_halt(regs, false);
}

void interrupt_handle_device_not_available(register_state_t* regs) {
	printf("Device not available\n");
	common_halt(regs, false);
}

void interrupt_handle_double_fault(register_state_t* regs) {
	printf("=======================\n");
	printf("Caught double fault\n");
	printf("=======================\n");
	common_halt(regs, false);
}

void interrupt_handle_invalid_tss(register_state_t* regs) {
	printf("Invalid TSS section\n");
	common_halt(regs, false);
}

void interrupt_handle_segment_not_present(register_state_t* regs) {
	printf("Segment not present\n");
	common_halt(regs, false);
}

void interrupt_handle_stack_segment_fault(register_state_t* regs) {
	printf("Stack segment fault\n");
	common_halt(regs, false);
}

void interrupt_handle_general_protection_fault(register_state_t* regs) {
	printf("General protection fault, error code %d\n", regs->err_code);
	common_halt(regs, false);
}

void interrupt_handle_floating_point_exception(register_state_t* regs) {
	printf("Floating point exception\n");
	common_halt(regs, false);
}

void interrupt_handle_alignment_check(register_state_t* regs) {
	printf("Alignment check\n");
	common_halt(regs, false);
}

void interrupt_handle_machine_check(register_state_t* regs) {
	printf("Machine check\n");
	common_halt(regs, false);
}

void interrupt_handle_virtualization_exception(register_state_t* regs) {
	printf("Virtualization exception\n");
	common_halt(regs, false);
}
