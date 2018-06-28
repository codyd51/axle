#ifndef INT_NOTIFIER_H
#define INT_NOTIFIER_H

#include <stdbool.h>
#include <stdint.h>

#include <std/std_base.h>
#include <kernel/interrupts/interrupts.h>

__BEGIN_DECLS

#define MAX_CALLBACKS 128

typedef struct {
    uint32_t int_no;
	void* func;
	void* context;
	bool repeats;
} int_notify_callback_t;

int_notify_callback_t* int_notifier_register_callback(uint32_t int_no, void* func, void* context, bool repeats);
void int_notifier_remove_callback(int_notify_callback_t* callback);

// Friend function for interrupt.c
void int_notifier_handle_interrupt(registers_t* register_state);

__END_DECLS

#endif // INT_NOTIFIER_H
