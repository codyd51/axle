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

/* Run the provided function when the the interrupt `int_no` is received. 
 * Callbacks set up by this method will only be invoked after the system has finished handling the interrupt.
 * If you provide `context`, it will be passed to the provided function on invocation.
 * Returns a struct encapsulating the callback, which you can use to remove it.
 */
int_notify_callback_t* int_notifier_register_callback(uint32_t int_no, void* func, void* context, bool repeats);

/* Stop invoking the provided callback.
 */
void int_notifier_remove_callback(int_notify_callback_t* callback);

// Friend function for interrupt.c
void int_notifier_handle_interrupt(registers_t* register_state);

__END_DECLS

#endif // INT_NOTIFIER_H
