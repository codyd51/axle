#ifndef INTERRUPT_HANDLERS_H
#define INTERRUPT_HANDLERS_H

#include "idt_structures.h"

void interrupt_handle_divide_by_zero(register_state_t regs);
void interrupt_handle_bound_range_exceeded(register_state_t regs);
void interrupt_handle_invalid_opcode(register_state_t regs);
void interrupt_handle_device_not_available(register_state_t regs);
void interrupt_handle_double_fault(register_state_t regs);
void interrupt_handle_invalid_tss(register_state_t regs);
void interrupt_handle_segment_not_present(register_state_t regs);
void interrupt_handle_stack_segment_fault(register_state_t regs);
void interrupt_handle_general_protection_fault(register_state_t regs);
void interrupt_handle_floating_point_exception(register_state_t regs);
void interrupt_handle_alignment_check(register_state_t regs);
void interrupt_handle_machine_check(register_state_t regs);
void interrupt_handle_virtualization_exception(register_state_t regs);

#endif
