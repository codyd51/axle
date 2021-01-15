#include "adi.h"
#include <std/math.h>
#include <kernel/interrupts/pic.h>
#include <kernel/util/spinlock/spinlock.h>
#include <kernel/multitasking/tasks/task_small.h>

#define MAX_IRQ 32
#define MAX_INT_VECTOR 128
static adi_driver_t _adi_drivers[MAX_IRQ] = {0};

static adi_driver_t* _adi_driver_matching_data(const char* name, task_small_t* task, uint32_t irq) {
    /*
     * Returns the first adi driver matching the provided data.
     * Provide an attribute of the service to be found, or NULL.
     */
    // Ensure at least 1 attribute is provided
    if (!name && !task && !irq) {
        panic("Must provide at least 1 attribute to match");
        return NULL;
    }

    for (uint32_t i = 0; i < sizeof(_adi_drivers) / sizeof(_adi_drivers[0]); i++) {
        adi_driver_t* driver = _adi_drivers + i;
        if (name != NULL && driver->name != NULL && !strcmp(driver->name, name)) {
            return driver;
        }
        if (task != NULL && driver->task != NULL && driver->task == task) {
            return driver;
        }
        if (irq != 0 && driver->irq != 0 && irq == driver->irq) {
            return driver;
        }
    }
    return NULL;
}

static void _adi_interrupt_handler(registers_t* regs) {
    // Wake up the driver thread that services this interrupt
    int irq = regs->int_no;
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task != NULL, "IRQ does not have a corresponding driver");

    adi_driver_t* driver = _adi_drivers + irq;
    driver->pending_irq_count += 1;
    task_small_t* task = (task_small_t*)driver->task;
    tasking_unblock_task_with_reason(task, false, IRQ_WAIT);
}

void adi_register_driver(const char* name, uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(!_adi_drivers[irq].task, "IRQ already mapped to a driver task");

    task_small_t* current_task = tasking_get_current_task();

    // We're modifying interrupt handling - clear interrupts
    spinlock_t int_spinlock = {0};
    if (!int_spinlock.name) int_spinlock.name = "adi_register_driver interrupt clear";
    spinlock_acquire(&int_spinlock);

    // Elevate the task's priority since it's a device driver
    spinlock_acquire(&current_task->priority_lock);
    current_task->priority = PRIORITY_DRIVER;
    spinlock_release(&current_task->priority_lock);

    _adi_drivers[irq].task = current_task;
    // The provided string is mapped into the address space of the running process,
    // but isn't mapped into kernel-space.
    // Copy the string so we can access it in kernel-space
    _adi_drivers[irq].name = strdup(name);

    // Set up an interrupt handler that will unblock the driver process
    interrupt_setup_callback(irq, _adi_interrupt_handler);

    spinlock_release(&int_spinlock);
}

bool adi_event_await(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task == tasking_get_current_task(), "Current task does not match driver layout");

    adi_driver_t* driver = _adi_drivers + irq;
    // If the driver has at least one interrupt to service now, don't block
    if (driver->pending_irq_count) {
        return true;
    }
    // The driver has re-entered its await-interrupt loop
    // Await the next interrupt or amc message
    task_state unblock_reason = tasking_block_task(driver->task, IRQ_WAIT|AMC_AWAIT_MESSAGE);
    // Make sure this was an event we're expecting
    assert (unblock_reason == IRQ_WAIT || unblock_reason == AMC_AWAIT_MESSAGE, "ADI driver awoke for unknown reason");
    return unblock_reason == IRQ_WAIT;
}

void adi_interrupt_dispatch(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    assert(_adi_drivers[irq].task != NULL, "IRQ does not have a corresponding driver");

    adi_driver_t* driver = _adi_drivers + irq;
    tasking_unblock_task(driver->task, false);
}

bool adi_services_interrupt(uint32_t irq) {
    assert(irq > 0 && irq < MAX_INT_VECTOR, "Invalid IRQ provided");
    return _adi_drivers[irq].task != NULL;
}
